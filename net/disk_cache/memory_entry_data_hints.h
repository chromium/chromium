// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_MEMORY_ENTRY_DATA_HINTS_H_
#define NET_DISK_CACHE_MEMORY_ENTRY_DATA_HINTS_H_

#include <stdint.h>

// On memory hint flags for the disk cache entry.
enum MemoryEntryDataHints : uint8_t {
  // If this hint is set, the caching headers indicate we can't do anything
  // with this entry (unless we are ignoring them thanks to a loadflag),
  // i.e. it's expired and has nothing that permits validations.
  HINT_UNUSABLE_PER_CACHING_HEADERS = (1 << 0),

  // If this hint is set, the entry will be cached with priority. So the
  // eviction policy will less likely evict the entry.
  HINT_HIGH_PRIORITY = (1 << 1),

  // Note: This hint flags are converted into a 2 bit field. So we can't have
  // more than 2 hints. This restriction comes from the fact that there are tens
  // of thousands of entries, and we need to keep the hint field size small.
};

#endif  // NET_DISK_CACHE_MEMORY_ENTRY_DATA_HINTS_H_
