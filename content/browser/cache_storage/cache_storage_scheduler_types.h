// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_SCHEDULER_TYPES_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_SCHEDULER_TYPES_H_

#include <stdint.h>

namespace content {

using CacheStorageSchedulerId = int64_t;

// Define the types of clients that might own a scheduler.  This enum is used
// to populate histogram names and must be kept in sync with the function
// in cache_storage_histogram_utils.cc.  Please keep this list sorted.  It is
// ok to renumber the enumeration since it is converted to a string and not
// directly recorded in the histogram.
enum class CacheStorageSchedulerClient {
  kCache,    // `CacheStorageCache`
  kStorage,  // `CacheStorage`
};

enum class CacheStorageSchedulerMode {
  kExclusive,  // Used for writes.
  kShared,     // Used for reads.
};

// Define the different types of operations that can be scheduled.  This enum
// is used to populate histogram names and must be kept in sync with the
// function in cache_storage_histogram_utils.cc.  Please keep this list sorted.
// It is ok to renumber the enumeration since it is converted to a string and
// not directly recorded in the histogram.
enum class CacheStorageSchedulerOp {
  kClose,
  kDelete,
  kGetAllMatched,
  kHas,
  kInit,
  kKeys,
  kMatch,
  kMatchAll,
  kOpen,
  kPut,
  kSize,
  kSizeThenClose,
  kTest,
  kWriteIndex,
  kWriteSideData,
};

enum class CacheStorageSchedulerPriority {
  kNormal,
  kHigh,
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_SCHEDULER_TYPES_H_
