// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HISTOGRAM_UTILS_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HISTOGRAM_UTILS_H_

#include "base/metrics/histogram_macros.h"
#include "content/browser/cache_storage/cache_storage_scheduler_client.h"
#include "third_party/blink/public/platform/modules/cache_storage/cache_storage.mojom.h"

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
  kMaxValue = kStorageGetAllMatchedEntriesBackendClosed,
};

blink::mojom::CacheStorageError MakeErrorStorage(ErrorStorageType type);

// Metrics to make it easier to write histograms for several clients.
#define CACHE_STORAGE_SCHEDULER_UMA_THUNK(uma_type, args) \
  UMA_HISTOGRAM_##uma_type args
#define CACHE_STORAGE_SCHEDULER_UMA(uma_type, uma_name, client_type, ...)     \
  do {                                                                        \
    switch (client_type) {                                                    \
      case CacheStorageSchedulerClient::CLIENT_STORAGE:                       \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK(                                    \
            uma_type, ("ServiceWorkerCache.CacheStorage.Scheduler." uma_name, \
                       ##__VA_ARGS__));                                       \
        break;                                                                \
      case CacheStorageSchedulerClient::CLIENT_CACHE:                         \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK(                                    \
            uma_type,                                                         \
            ("ServiceWorkerCache.Cache.Scheduler." uma_name, ##__VA_ARGS__)); \
        break;                                                                \
      case CacheStorageSchedulerClient::CLIENT_BACKGROUND_SYNC:               \
        CACHE_STORAGE_SCHEDULER_UMA_THUNK(                                    \
            uma_type,                                                         \
            ("ServiceWorkerCache.BackgroundSyncManager.Scheduler." uma_name,  \
             ##__VA_ARGS__));                                                 \
        break;                                                                \
      default:                                                                \
        NOTREACHED();                                                         \
        break;                                                                \
    }                                                                         \
  } while (0)

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_HISTOGRAM_UTILS_H_
