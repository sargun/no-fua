#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/device-mapper.h>

#define DM_MSG_PREFIX "no-fua"

struct no_fua {
	struct dm_dev	*dev;
};

static int no_fua_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct dm_arg_set as;
	const char *devname;
	struct no_fua *nf;
	int ret;

	as.argc = argc;
	as.argv = argv;

	if (argc != 1) {
		ti->error = "Invalid argument count";
		return -EINVAL;
	}

	nf = kzalloc(sizeof(*nf), GFP_KERNEL);
	if (!nf) {
		ti->error = "Cannot allocate context";
		return -ENOMEM;
	}

	devname = dm_shift_arg(&as);
	ret = dm_get_device(ti, devname, dm_table_get_mode(ti->table), &nf->dev);
	if (ret) {
		ti->error = "Device lookup failed";
		kfree(nf);
		return ret;
	}

	ti->num_flush_bios = 1;
	ti->num_discard_bios = 1;
	ti->num_write_same_bios = 1;
	ti->num_write_zeroes_bios = 1;

	ti->private = nf;

	return 0;
}

static void no_fua_dtr(struct dm_target *ti)
{
	struct no_fua *nf = (struct no_fua*)ti->private;

	dm_put_device(ti, nf->dev);
	kfree(nf);
}

static int no_fua_map(struct dm_target *ti, struct bio *bio)
{
	struct no_fua *nf = (struct no_fua*)ti->private;

	bio_set_dev(bio, nf->dev->bdev);
	bio->bi_opf &= ~(REQ_FUA|REQ_PREFLUSH);

	return DM_MAPIO_REMAPPED;
}

static int no_fua_end_io(struct dm_target *ti, struct bio *bio,
			 blk_status_t *error)
{
	return DM_ENDIO_DONE;
}


static void no_fua_status(struct dm_target *ti, status_type_t type,
			  unsigned status_flags, char *result, unsigned maxlen)
{
	struct no_fua *nf = (struct no_fua*)ti->private;

	switch (type) {
	case STATUSTYPE_INFO:
		result[0] = '\0';
		break;
	case STATUSTYPE_TABLE:
		snprintf(result, maxlen, "%s", nf->dev->name);
		break;
	}
}

static int no_fua_iterate_devices(struct dm_target *ti,
				  iterate_devices_callout_fn fn, void *data)
{
	struct no_fua *nf = (struct no_fua*)ti->private;

	return fn(ti, nf->dev, 0, ti->len, data);
}

static struct target_type no_fua_target = {
	.name			= "no-fua",
	.version		= {1, 0, 0},
	.features		= DM_TARGET_PASSES_INTEGRITY | DM_TARGET_ZONED_HM,
	.module			= THIS_MODULE,
	.ctr			= no_fua_ctr,
	.dtr			= no_fua_dtr,
	.map			= no_fua_map,
	.end_io			= no_fua_end_io,
	.status			= no_fua_status,
	.iterate_devices	= no_fua_iterate_devices,
};

int init_module(void)
{
	int ret = dm_register_target(&no_fua_target);

	if (ret < 0)
		DMERR("register failed %d", ret);

	return ret;
}

void cleanup_module(void)
{
	dm_unregister_target(&no_fua_target);
}

MODULE_LICENSE("GPL");
