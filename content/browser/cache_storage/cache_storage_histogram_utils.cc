// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"

namespace content {

blink::mojom::CacheStorageError MakeErrorStorage(ErrorStorageType type) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorkerCache.ErrorStorageType", type);
  return blink::mojom::CacheStorageError::kErrorStorage;
}

namespace {

// Helper macro to return a literal base::StringPiece.  Once
// we can use c++17 we can use string view literals instead.
#define RETURN_LITERAL_STRING_PIECE(target)                \
  do {                                                     \
    static constexpr base::StringPiece kValue("." target); \
    return kValue;                                         \
  } while (0)

base::StringPiece UMAToName(CacheStorageSchedulerUMA uma_type) {
  switch (uma_type) {
    case CacheStorageSchedulerUMA::kOperationDuration:
      RETURN_LITERAL_STRING_PIECE("OperationDuration2");
    case CacheStorageSchedulerUMA::kQueueDuration:
      RETURN_LITERAL_STRING_PIECE("QueueDuration2");
    case CacheStorageSchedulerUMA::kQueueLength:
      RETURN_LITERAL_STRING_PIECE("QueueLength");
  }
}

base::StringPiece ClientToName(CacheStorageSchedulerClient client_type) {
  switch (client_type) {
    case CacheStorageSchedulerClient::kBackgroundSync:
      RETURN_LITERAL_STRING_PIECE("BackgroundSyncManager");
    case CacheStorageSchedulerClient::kCache:
      RETURN_LITERAL_STRING_PIECE("Cache");
    case CacheStorageSchedulerClient::kStorage:
      RETURN_LITERAL_STRING_PIECE("CacheStorage");
  }
}

bool ShouldRecordOpUMA(CacheStorageSchedulerOp op_type) {
  return op_type != CacheStorageSchedulerOp::kBackgroundSync &&
         op_type != CacheStorageSchedulerOp::kTest;
}

base::StringPiece OpToName(CacheStorageSchedulerOp op_type) {
  switch (op_type) {
    case CacheStorageSchedulerOp::kBackgroundSync:
      NOTREACHED();
      return "";
    case CacheStorageSchedulerOp::kClose:
      RETURN_LITERAL_STRING_PIECE("Close");
    case CacheStorageSchedulerOp::kDelete:
      RETURN_LITERAL_STRING_PIECE("Delete");
    case CacheStorageSchedulerOp::kGetAllMatched:
      RETURN_LITERAL_STRING_PIECE("GetAllMatched");
    case CacheStorageSchedulerOp::kHas:
      RETURN_LITERAL_STRING_PIECE("Has");
    case CacheStorageSchedulerOp::kInit:
      RETURN_LITERAL_STRING_PIECE("Init");
    case CacheStorageSchedulerOp::kKeys:
      RETURN_LITERAL_STRING_PIECE("Keys");
    case CacheStorageSchedulerOp::kMatch:
      RETURN_LITERAL_STRING_PIECE("Match");
    case CacheStorageSchedulerOp::kMatchAll:
      RETURN_LITERAL_STRING_PIECE("MatchAll");
    case CacheStorageSchedulerOp::kOpen:
      RETURN_LITERAL_STRING_PIECE("Open");
    case CacheStorageSchedulerOp::kPut:
      RETURN_LITERAL_STRING_PIECE("Put");
    case CacheStorageSchedulerOp::kSize:
      RETURN_LITERAL_STRING_PIECE("Size");
    case CacheStorageSchedulerOp::kSizeThenClose:
      RETURN_LITERAL_STRING_PIECE("SizeThenClose");
    case CacheStorageSchedulerOp::kTest:
      NOTREACHED();
      return "";
    case CacheStorageSchedulerOp::kWriteIndex:
      RETURN_LITERAL_STRING_PIECE("WriteIndex");
    case CacheStorageSchedulerOp::kWriteSideData:
      RETURN_LITERAL_STRING_PIECE("WriteSideData");
  }
}

std::string GetClientHistogramName(CacheStorageSchedulerUMA uma_type,
                                   CacheStorageSchedulerClient client_type) {
  std::string histogram_name("ServiceWorkerCache");
  histogram_name.append(std::string(ClientToName(client_type)));
  histogram_name.append(".Scheduler");
  histogram_name.append(std::string(UMAToName(uma_type)));
  return histogram_name;
}

}  // namespace

void RecordCacheStorageSchedulerUMA(CacheStorageSchedulerUMA uma_type,
                                    CacheStorageSchedulerClient client_type,
                                    CacheStorageSchedulerOp op_type,
                                    int value) {
  DCHECK(uma_type == CacheStorageSchedulerUMA::kQueueLength);
  DCHECK(client_type != CacheStorageSchedulerClient::kBackgroundSync ||
         op_type == CacheStorageSchedulerOp::kBackgroundSync);
  std::string histogram_name = GetClientHistogramName(uma_type, client_type);
  base::UmaHistogramCounts10000(histogram_name, value);
  if (!ShouldRecordOpUMA(op_type))
    return;
  histogram_name.append(std::string(OpToName(op_type)));
  base::UmaHistogramCounts10000(histogram_name, value);
}

void RecordCacheStorageSchedulerUMA(CacheStorageSchedulerUMA uma_type,
                                    CacheStorageSchedulerClient client_type,
                                    CacheStorageSchedulerOp op_type,
                                    base::TimeDelta value) {
  DCHECK(uma_type == CacheStorageSchedulerUMA::kOperationDuration ||
         uma_type == CacheStorageSchedulerUMA::kQueueDuration);
  DCHECK(client_type != CacheStorageSchedulerClient::kBackgroundSync ||
         op_type == CacheStorageSchedulerOp::kBackgroundSync);
  std::string histogram_name = GetClientHistogramName(uma_type, client_type);
  base::UmaHistogramLongTimes(histogram_name, value);
  if (!ShouldRecordOpUMA(op_type))
    return;
  histogram_name.append(std::string(OpToName(op_type)));
  base::UmaHistogramLongTimes(histogram_name, value);
}

}  // namespace content
