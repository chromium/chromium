// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/stats.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/sample_vector.h"
#include "base/metrics/statistics_recorder.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace {

const int32_t kDiskSignature = 0xF01427E0;

struct OnDiskStats {
  int32_t signature;
  int size;
  int data_sizes[disk_cache::Stats::kDataSizesLength];
  int64_t counters[disk_cache::Stats::MAX_COUNTER];
};
static_assert(sizeof(OnDiskStats) < 512, "needs more than 2 blocks");

// Returns the "floor" (as opposed to "ceiling") of log base 2 of number.
int LogBase2(int32_t number) {
  unsigned int value = static_cast<unsigned int>(number);
  const unsigned int mask[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
  const unsigned int s[] = {1, 2, 4, 8, 16};

  unsigned int result = 0;
  for (int i = 4; i >= 0; i--) {
    if (value & mask[i]) {
      value >>= s[i];
      result |= s[i];
    }
  }
  return static_cast<int>(result);
}

// WARNING: Add new stats only at the end, or change LoadStats().
const char* const kCounterNames[] = {
  "Open miss",
  "Open hit",
  "Create miss",
  "Create hit",
  "Resurrect hit",
  "Create error",
  "Trim entry",
  "Doom entry",
  "Doom cache",
  "Invalid entry",
  "Open entries",
  "Max entries",
  "Timer",
  "Read data",
  "Write data",
  "Open rankings",
  "Get rankings",
  "Fatal error",
  "Last report",
  "Last report timer",
  "Doom recent entries",
  "unused"
};
static_assert(base::size(kCounterNames) == disk_cache::Stats::MAX_COUNTER,
              "update the names");

}  // namespace

namespace disk_cache {

bool VerifyStats(OnDiskStats* stats) {
  if (stats->signature != kDiskSignature)
    return false;

  // We don't want to discard the whole cache every time we have one extra
  // counter; we keep old data if we can.
  if (static_cast<unsigned int>(stats->size) > sizeof(*stats)) {
    memset(stats, 0, sizeof(*stats));
    stats->signature = kDiskSignature;
  } else if (static_cast<unsigned int>(stats->size) != sizeof(*stats)) {
    size_t delta = sizeof(*stats) - static_cast<unsigned int>(stats->size);
    memset(reinterpret_cast<char*>(stats) + stats->size, 0, delta);
    stats->size = sizeof(*stats);
  }

  return true;
}

Stats::Stats() = default;

Stats::~Stats() = default;

bool Stats::Init(void* data, int num_bytes, Addr address) {
  OnDiskStats local_stats;
  OnDiskStats* stats = &local_stats;
  if (!num_bytes) {
    memset(stats, 0, sizeof(local_stats));
    local_stats.signature = kDiskSignature;
    local_stats.size = sizeof(local_stats);
  } else if (num_bytes >= static_cast<int>(sizeof(*stats))) {
    stats = reinterpret_cast<OnDiskStats*>(data);
    if (!VerifyStats(stats)) {
      memset(&local_stats, 0, sizeof(local_stats));
      if (memcmp(stats, &local_stats, sizeof(local_stats))) {
        return false;
      } else {
        // The storage is empty which means that SerializeStats() was never
        // called on the last run. Just re-initialize everything.
        local_stats.signature = kDiskSignature;
        local_stats.size = sizeof(local_stats);
        stats = &local_stats;
      }
    }
  } else {
    return false;
  }

  storage_addr_ = address;

  memcpy(data_sizes_, stats->data_sizes, sizeof(data_sizes_));
  memcpy(counters_, stats->counters, sizeof(counters_));

  // Clean up old value.
  SetCounter(UNUSED, 0);
  return true;
}

void Stats::InitSizeHistogram() {
  // Only generate this histogram for the main cache.
  static bool first_time = true;
  if (!first_time)
    return;

  first_time = false;
  for (int i = 0; i < kDataSizesLength; i++) {
    // This is a good time to fix any inconsistent data. The count should be
    // always positive, but if it's not, reset the value now.
    if (data_sizes_[i] < 0)
      data_sizes_[i] = 0;
  }
}

int Stats::StorageSize() {
  // If we have more than 512 bytes of counters, change kDiskSignature so we
  // don't overwrite something else (LoadStats must fail).
  static_assert(sizeof(OnDiskStats) <= 256 * 2, "use more blocks");
  return 256 * 2;
}

void Stats::ModifyStorageStats(int32_t old_size, int32_t new_size) {
  // We keep a counter of the data block size on an array where each entry is
  // the adjusted log base 2 of the size. The first entry counts blocks of 256
  // bytes, the second blocks up to 512 bytes, etc. With 20 entries, the last
  // one stores entries of more than 64 MB
  int new_index = GetStatsBucket(new_size);
  int old_index = GetStatsBucket(old_size);

  if (new_size)
    data_sizes_[new_index]++;

  if (old_size)
    data_sizes_[old_index]--;
}

void Stats::OnEvent(Counters an_event) {
  DCHECK(an_event >= MIN_COUNTER && an_event < MAX_COUNTER);
  counters_[an_event]++;
}

void Stats::SetCounter(Counters counter, int64_t value) {
  DCHECK(counter >= MIN_COUNTER && counter < MAX_COUNTER);
  counters_[counter] = value;
}

int64_t Stats::GetCounter(Counters counter) const {
  DCHECK(counter >= MIN_COUNTER && counter < MAX_COUNTER);
  return counters_[counter];
}

void Stats::GetItems(StatsItems* items) {
  std::pair<std::string, std::string> item;
  for (int i = 0; i < kDataSizesLength; i++) {
    item.first = base::StringPrintf("Size%02d", i);
    item.second = base::StringPrintf("0x%08x", data_sizes_[i]);
    items->push_back(item);
  }

  for (int i = MIN_COUNTER; i < MAX_COUNTER; i++) {
    item.first = kCounterNames[i];
    item.second = base::StringPrintf("0x%" PRIx64, counters_[i]);
    items->push_back(item);
  }
}

int Stats::GetHitRatio() const {
  return GetRatio(OPEN_HIT, OPEN_MISS);
}

int Stats::GetResurrectRatio() const {
  return GetRatio(RESURRECT_HIT, CREATE_HIT);
}

void Stats::ResetRatios() {
  SetCounter(OPEN_HIT, 0);
  SetCounter(OPEN_MISS, 0);
  SetCounter(RESURRECT_HIT, 0);
  SetCounter(CREATE_HIT, 0);
}

int Stats::GetLargeEntriesSize() {
  int total = 0;
  // data_sizes_[20] stores values between 512 KB and 1 MB (see comment before
  // GetStatsBucket()).
  for (int bucket = 20; bucket < kDataSizesLength; bucket++)
    total += data_sizes_[bucket] * GetBucketRange(bucket);

  return total;
}

int Stats::SerializeStats(void* data, int num_bytes, Addr* address) {
  OnDiskStats* stats = reinterpret_cast<OnDiskStats*>(data);
  if (num_bytes < static_cast<int>(sizeof(*stats)))
    return 0;

  stats->signature = kDiskSignature;
  stats->size = sizeof(*stats);
  memcpy(stats->data_sizes, data_sizes_, sizeof(data_sizes_));
  memcpy(stats->counters, counters_, sizeof(counters_));

  *address = storage_addr_;
  return sizeof(*stats);
}

int Stats::GetBucketRange(size_t i) const {
  if (i < 2)
    return static_cast<int>(1024 * i);

  if (i < 12)
    return static_cast<int>(2048 * (i - 1));

  if (i < 17)
    return static_cast<int>(4096 * (i - 11)) + 20 * 1024;

  int n = 64 * 1024;
  if (i > static_cast<size_t>(kDataSizesLength)) {
    NOTREACHED();
    i = kDataSizesLength;
  }

  i -= 17;
  n <<= i;
  return n;
}

// The array will be filled this way:
//  index      size
//    0       [0, 1024)
//    1    [1024, 2048)
//    2    [2048, 4096)
//    3      [4K, 6K)
//      ...
//   10     [18K, 20K)
//   11     [20K, 24K)
//   12     [24k, 28K)
//      ...
//   15     [36k, 40K)
//   16     [40k, 64K)
//   17     [64K, 128K)
//   18    [128K, 256K)
//      ...
//   23      [4M, 8M)
//   24      [8M, 16M)
//   25     [16M, 32M)
//   26     [32M, 64M)
//   27     [64M, ...)
int Stats::GetStatsBucket(int32_t size) {
  if (size < 1024)
    return 0;

  // 10 slots more, until 20K.
  if (size < 20 * 1024)
    return size / 2048 + 1;

  // 5 slots more, from 20K to 40K.
  if (size < 40 * 1024)
    return (size - 20 * 1024) / 4096 + 11;

  // From this point on, use a logarithmic scale.
  int result =  LogBase2(size) + 1;

  static_assert(kDataSizesLength > 16, "update the scale");
  if (result >= kDataSizesLength)
    result = kDataSizesLength - 1;

  return result;
}

int Stats::GetRatio(Counters hit, Counters miss) const {
  int64_t ratio = GetCounter(hit) * 100;
  if (!ratio)
    return 0;

  ratio /= (GetCounter(hit) + GetCounter(miss));
  return static_cast<int>(ratio);
}

}  // namespace disk_cache
