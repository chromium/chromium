// Copyright 2016 The Chromium Authors. All rights reserved.
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
  kBackgroundSync = 0,
  kCache = 1,
  kStorage = 2,
};

enum class CacheStorageSchedulerMode {
  kExclusive,
  kShared,
};

// Define the different types of operations that can be scheduled.  This enum
// is used to populate histogram names and must be kept in sync with the
// function in cache_storage_histogram_utils.cc.  Please keep this list sorted.
// It is ok to renumber the enumeration since it is converted to a string and
// not directly recorded in the histogram.
enum class CacheStorageSchedulerOp {
  kBackgroundSync = 0,
  kClose = 1,
  kDelete = 2,
  kGetAllMatched = 3,
  kHas = 4,
  kInit = 5,
  kKeys = 6,
  kMatch = 7,
  kMatchAll = 8,
  kOpen = 9,
  kPut = 10,
  kSize = 11,
  kSizeThenClose = 12,
  kTest = 13,
  kWriteIndex = 14,
  kWriteSideData = 15,
};

enum class CacheStorageSchedulerPriority {
  kNormal = 0,
  kHigh = 1,
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_SCHEDULER_TYPES_H_
