// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HISTOGRAM_UTILS_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HISTOGRAM_UTILS_H_

#include "content/browser/cache_storage/cache_storage_scheduler_types.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace content {

// This enum gets recorded as a histogram.  Do not renumber the values.
enum class ErrorStorageType {
  kDidCreateNullCache = 0,
  kDeleteCacheFailed = 1,
  kMatchBackendClosed = 2,
  kMatchAllBackendClosed = 3,
  kWriteSideDataBackendClosed = 4,
  kBatchBackendClosed = 5,
  kBatchInvalidSpace = 6,
  kBatchDidGetUsageAndQuotaInvalidSpace = 7,
  kBatchDidGetUsageAndQuotaUndefinedOp = 8,
  kKeysBackendClosed = 9,
  kQueryCacheBackendClosed = 10,
  kQueryCacheFilterEntryFailed = 11,
  kQueryCacheDidReadMetadataNullBlobContext = 12,
  kStorageMatchAllBackendClosed = 13,
  kWriteSideDataImplBackendClosed = 14,
  kPutImplBackendClosed = 15,
  kPutDidDeleteEntryBackendClosed = 16,
  kMetadataSerializationFailed = 17,
  kPutDidWriteHeadersWrongBytes = 18,
  kPutDidWriteBlobToCacheFailed = 19,
  kDeleteImplBackendClosed = 20,
  kKeysImplBackendClosed = 21,
  kCreateBackendDidCreateFailed = 22,
  kStorageGetAllMatchedEntriesBackendClosed = 23,
  kStorageHandleNull = 24,
  kWriteSideDataDidWriteMetadataWrongBytes = 25,
  kDefaultBucketError = 26,
  kMaxValue = kDefaultBucketError,
};

blink::mojom::CacheStorageError MakeErrorStorage(ErrorStorageType type);

enum class CacheStorageSchedulerUMA {
  kOperationDuration = 0,
  kQueueDuration = 1,
  kQueueLength = 2,
};

// The following functions are used to record UMA histograms for the
// scheduler.  There are two functions to handle the different argument types
// without triggering template code bloat.
void RecordCacheStorageSchedulerUMA(CacheStorageSchedulerUMA uma_type,
                                    CacheStorageSchedulerClient client_type,
                                    CacheStorageSchedulerOp op_type,
                                    int value);

void RecordCacheStorageSchedulerUMA(CacheStorageSchedulerUMA uma_type,
                                    CacheStorageSchedulerClient client_type,
                                    CacheStorageSchedulerOp op_type,
                                    base::TimeDelta value);

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HISTOGRAM_UTILS_H_
