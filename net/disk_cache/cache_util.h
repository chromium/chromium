// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_CACHE_UTIL_H_
#define NET_DISK_CACHE_CACHE_UTIL_H_

#include <stdint.h>

#include "base/feature_list.h"
#include "net/base/net_export.h"
#include "net/disk_cache/disk_cache.h"

namespace base {
class FilePath;
}

namespace disk_cache {

// Experiment to increase the cache size to see the impact on various
// performance metrics.
NET_EXPORT_PRIVATE extern const base::Feature kChangeDiskCacheSizeExperiment;

// Moves the cache files from the given path to another location.
// Fails if the destination exists already, or if it doesn't have
// permission for the operation.  This is basically a rename operation
// for the cache directory.  Returns true if successful.  On ChromeOS,
// this moves the cache contents, and leaves the empty cache
// directory.
NET_EXPORT_PRIVATE bool MoveCache(const base::FilePath& from_path,
                                  const base::FilePath& to_path);

// Deletes the cache files stored on |path|, and optionally also attempts to
// delete the folder itself.
NET_EXPORT_PRIVATE void DeleteCache(const base::FilePath& path,
                                    bool remove_folder);

// Deletes a cache file.
NET_EXPORT_PRIVATE bool DeleteCacheFile(const base::FilePath& name);

// Renames cache directory synchronously and fires off a background cleanup
// task. Used by cache creator itself or by backends for self-restart on error.
bool DelayedCacheCleanup(const base::FilePath& full_path);

// Returns the preferred max cache size given the available disk space and
// cache type.
NET_EXPORT_PRIVATE int PreferredCacheSize(
    int64_t available,
    net::CacheType type = net::DISK_CACHE);

// The default cache size should not ideally be exposed, but the blockfile
// backend uses it for reasons that include testing.
NET_EXPORT_PRIVATE extern const int kDefaultCacheSize;

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_CACHE_UTIL_H_
