// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_STATS_H_
#define NET_DISK_CACHE_BLOCKFILE_STATS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string_split.h"
#include "net/base/net_export.h"
#include "net/disk_cache/blockfile/addr.h"

namespace disk_cache {

using StatsItems = base::StringPairs;

// This class stores cache-specific usage information, for tunning purposes.
class NET_EXPORT_PRIVATE Stats {
 public:
  static const int kDataSizesLength = 28;
  enum Counters {
    MIN_COUNTER = 0,
    OPEN_MISS = MIN_COUNTER,
    OPEN_HIT,
    CREATE_MISS,
    CREATE_HIT,
    RESURRECT_HIT,
    CREATE_ERROR,
    TRIM_ENTRY,
    DOOM_ENTRY,
    DOOM_CACHE,
    INVALID_ENTRY,
    OPEN_ENTRIES,  // Average number of open entries.
    MAX_ENTRIES,  // Maximum number of open entries.
    TIMER,
    READ_DATA,
    WRITE_DATA,
    OPEN_RANKINGS,  // An entry has to be read just to modify rankings.
    GET_RANKINGS,  // We got the ranking info without reading the whole entry.
    FATAL_ERROR,
    LAST_REPORT,  // Time of the last time we sent a report.
    LAST_REPORT_TIMER,  // Timer count of the last time we sent a report.
    DOOM_RECENT,  // The cache was partially cleared.
    UNUSED,  // Was: ga.js was evicted from the cache.
    MAX_COUNTER
  };

  Stats();

  Stats(const Stats&) = delete;
  Stats& operator=(const Stats&) = delete;

  ~Stats();

  // Initializes this object with |data| from disk.
  bool Init(void* data, int num_bytes, Addr address);

  // Generates a size distribution histogram.
  void InitSizeHistogram();

  // Returns the number of bytes needed to store the stats on disk.
  int StorageSize();

  // Tracks changes to the stoage space used by an entry.
  void ModifyStorageStats(int32_t old_size, int32_t new_size);

  // Tracks general events.
  void OnEvent(Counters an_event);
  void SetCounter(Counters counter, int64_t value);
  int64_t GetCounter(Counters counter) const;

  void GetItems(StatsItems* items);
  void ResetRatios();

  // Returns the lower bound of the space used by entries bigger than 512 KB.
  int GetLargeEntriesSize();

  // Writes the stats into |data|, to be stored at the given cache address.
  // Returns the number of bytes copied.
  int SerializeStats(void* data, int num_bytes, Addr* address);

 private:
  // Supports generation of SizeStats histogram data.
  int GetBucketRange(size_t i) const;
  int GetStatsBucket(int32_t size);
  int GetRatio(Counters hit, Counters miss) const;

  Addr storage_addr_;
  int data_sizes_[kDataSizesLength];
  int64_t counters_[MAX_COUNTER];
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_STATS_H_
