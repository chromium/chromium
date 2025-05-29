// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_VERSION_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_VERSION_H_

#include <stdint.h>

namespace disk_cache {

// Short rules helping to think about data upgrades within Simple Cache:
//   * ALL changes of on-disk data format, backward-compatible or not,
//     forward-compatible or not, require updating the `kSimpleVersion`.
//   * Update `kSimpleIndexFileVersion`, `kSimpleSparseEntryVersion` or
//     `kSimpleEntryVersionOnDisk` which corresponds to the upgraded format.
//   * All cache Upgrades are performed on backend start, must be finished
//     before the new backend starts processing any incoming operations.
//   * If the Upgrade is not implemented for transition from
//     `kSimpleVersion - 1` then the whole cache directory will be cleared.
//   * Dropping cache data on disk or some of its parts can be a valid way to
//     Upgrade.
//
// Use `kSimpleVersion` for the fake index file.
inline constexpr uint32_t kSimpleVersion = 9;

// The version of the index file. Must be updated iff the index format changes.
inline constexpr uint32_t kSimpleIndexFileVersion = 9;

// Minimum version to support upgrade for the index file.
inline constexpr uint32_t kMinSimpleIndexFileVersionSupported = 8;

// The version of the sparse entry file. Must be updated iff the sparse file
// format changes.
inline constexpr uint32_t kSimpleSparseEntryVersion = 9;

// The version of the entry file(s) as written to disk. Must be updated iff the
// entry format changes.
inline constexpr uint32_t kSimpleEntryVersionOnDisk = 5;

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_BACKEND_VERSION_H_
