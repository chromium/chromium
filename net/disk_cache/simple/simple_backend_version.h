// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_VERSION_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_VERSION_H_

#include <stdint.h>

namespace disk_cache {

// Short rules helping to think about data upgrades within Simple Cache:
//   * ALL changes of on-disk data format, backward-compatible or not,
//     forward-compatible or not, require updating the |kSimpleVersion|.
//   * All cache Upgrades are performed on backend start, must be finished
//     before the new backend starts processing any incoming operations.
//   * If the Upgrade is not implemented for transition from
//     |kSimpleVersion - 1| then the whole cache directory will be cleared.
//   * Dropping cache data on disk or some of its parts can be a valid way to
//     Upgrade.
const uint32_t kLastCompatSparseVersion = 7;
const uint32_t kSimpleVersion = 9;

// The version of the entry file(s) as written to disk. Must be updated iff the
// entry format changes with the overall backend version update.
const uint32_t kSimpleEntryVersionOnDisk = 5;

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_VERSION_H_
