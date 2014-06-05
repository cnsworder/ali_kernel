/*
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
 *
 * This file is released under the GPL.
 */

#include <linux/sysfs.h>
#include <asm/atomic.h>
#include <linux/dm-ioctl.h>
#include "dm.h"

struct dm_sysfs_attr {
	struct attribute attr;
	ssize_t (*show)(struct mapped_device *, char *);
	ssize_t (*store)(struct mapped_device *, const char *, size_t len);
};

#define DM_ATTR_RO(_name) \
struct dm_sysfs_attr dm_attr_##_name = \
	__ATTR(_name, S_IRUGO, dm_attr_##_name##_show, NULL)

#define DM_ATTR_RW(_name) \
struct dm_sysfs_attr dm_attr_##_name = \
	__ATTR(_name, S_IRUGO | S_IWUSR, NULL, dm_attr_##_name##_store)

static ssize_t dm_attr_show(struct kobject *kobj, struct attribute *attr,
			    char *page)
{
	struct dm_sysfs_attr *dm_attr;
	struct mapped_device *md;
	ssize_t ret;

	dm_attr = container_of(attr, struct dm_sysfs_attr, attr);
	if (!dm_attr->show)
		return -EIO;

	md = dm_get_from_kobject(kobj);
	if (!md)
		return -EINVAL;

	ret = dm_attr->show(md, page);
	dm_put(md);

	return ret;
}

static ssize_t dm_attr_store(struct kobject *kobj, struct attribute *attr,
			     const char *page, size_t len)
{
	struct dm_sysfs_attr *dm_attr;
	struct mapped_device *md;
	ssize_t ret;

	dm_attr = container_of(attr, struct dm_sysfs_attr, attr);
	if (!dm_attr->store)
		return -EIO;

	md = dm_get_from_kobject(kobj);
	if (!md)
		return -EINVAL;

	ret = dm_attr->store(md, page, len);
	dm_put(md);

	return ret;
}

static ssize_t dm_attr_name_show(struct mapped_device *md, char *buf)
{
	if (dm_copy_name_and_uuid(md, buf, NULL))
		return -EIO;

	strcat(buf, "\n");
	return strlen(buf);
}

static ssize_t dm_attr_uuid_show(struct mapped_device *md, char *buf)
{
	if (dm_copy_name_and_uuid(md, NULL, buf))
		return -EIO;

	strcat(buf, "\n");
	return strlen(buf);
}

static ssize_t dm_attr_suspended_show(struct mapped_device *md, char *buf)
{
	sprintf(buf, "%d\n", dm_suspended_md(md));

	return strlen(buf);
}

static ssize_t dm_attr_io_latency_us_show(struct mapped_device *md, char *buf)
{
	int slot_base = 0;
	int i, nr, ptr;

	for (ptr = 0, i = 0; i < DM_LATENCY_STATS_US_NR; i++) {
		nr = sprintf(buf + ptr,
			     "%d-%d(us):%d\n",
			     slot_base,
			     slot_base + DM_LATENCY_STATS_US_GRAINSIZE - 1,
			     atomic_read(&md->latency_stats_us[i]));
		if (nr < 0)
			break;

		slot_base += DM_LATENCY_STATS_US_GRAINSIZE;
		ptr += nr;
	}

	return strlen(buf);
}


static ssize_t dm_attr_io_latency_ms_show(struct mapped_device *md, char *buf)
{
	int slot_base = 0;
	int i, nr, ptr;

	for (ptr = 0, i = 0; i < DM_LATENCY_STATS_MS_NR; i++) {
		nr = sprintf(buf + ptr,
			     "%d-%d(ms):%d\n",
			     slot_base,
			     slot_base + DM_LATENCY_STATS_MS_GRAINSIZE - 1,
			     atomic_read(&(md->latency_stats_ms[i])));
		if (nr < 0)
			break;

		slot_base += DM_LATENCY_STATS_MS_GRAINSIZE;
		ptr += nr;
	}

	return strlen(buf);
}

static ssize_t dm_attr_io_latency_s_show(struct mapped_device *md, char *buf)
{
	int slot_base = 0;
	int i, nr , ptr;

	for(ptr = 0, i = 0; i < DM_LATENCY_STATS_S_NR; i++) {
		nr = sprintf(buf + ptr,
			      "%d-%d(s):%d\n",
			      slot_base,
			      slot_base + DM_LATENCY_STATS_S_GRAINSIZE - 1,
			      atomic_read(&(md->latency_stats_s[i])));
		if (nr < 0)
			break;

		slot_base += DM_LATENCY_STATS_S_GRAINSIZE;
		ptr += nr;
	}

	return strlen(buf);
}

static ssize_t dm_attr_io_latency_reset_store(struct mapped_device *md,
					      const char *buf,
					      size_t len)
{
	int i;
	for (i = 0; i < DM_LATENCY_STATS_US_NR; i++)
		atomic_set(&md->latency_stats_us[i], 0);
	for (i = 0; i < DM_LATENCY_STATS_MS_NR; i++)
		atomic_set(&md->latency_stats_ms[i], 0);
	for (i = 0; i < DM_LATENCY_STATS_S_NR; i++)
		atomic_set(&md->latency_stats_s[i], 0);
	return len;
}

static DM_ATTR_RO(name);
static DM_ATTR_RO(uuid);
static DM_ATTR_RO(suspended);
static DM_ATTR_RO(io_latency_us);
static DM_ATTR_RO(io_latency_ms);
static DM_ATTR_RO(io_latency_s);
static DM_ATTR_RW(io_latency_reset);

static struct attribute *dm_attrs[] = {
	&dm_attr_name.attr,
	&dm_attr_uuid.attr,
	&dm_attr_suspended.attr,
	&dm_attr_io_latency_us.attr,
	&dm_attr_io_latency_ms.attr,
	&dm_attr_io_latency_s.attr,
	&dm_attr_io_latency_reset.attr,
	NULL,
};

static const struct sysfs_ops dm_sysfs_ops = {
	.show	= dm_attr_show,
	.store	= dm_attr_store,
};

/*
 * The sysfs structure is embedded in md struct, nothing to do here
 */
static void dm_sysfs_release(struct kobject *kobj)
{
}

/*
 * dm kobject is embedded in mapped_device structure
 * no need to define release function here
 */
static struct kobj_type dm_ktype = {
	.sysfs_ops	= &dm_sysfs_ops,
	.default_attrs	= dm_attrs,
	.release	= dm_sysfs_release
};

/*
 * Initialize kobj
 * because nobody using md yet, no need to call explicit dm_get/put
 */
int dm_sysfs_init(struct mapped_device *md)
{
	return kobject_init_and_add(dm_kobject(md), &dm_ktype,
				    &disk_to_dev(dm_disk(md))->kobj,
				    "%s", "dm");
}

/*
 * Remove kobj, called after all references removed
 */
void dm_sysfs_exit(struct mapped_device *md)
{
	kobject_put(dm_kobject(md));
}
