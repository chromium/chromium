// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

bool IsValidSharedStorageURLsArrayLength(size_t length) {
  return length > 0u &&
         length <=
             static_cast<size_t>(
                 features::kSharedStorageURLSelectionOperationInputURLSizeLimit
                     .Get());
}

bool IsValidSharedStorageKeyStringLength(size_t length) {
  return length > 0u &&
         length <=
             static_cast<size_t>(features::kMaxSharedStorageStringLength.Get());
}

bool IsValidSharedStorageValueStringLength(size_t length) {
  return length <=
         static_cast<size_t>(features::kMaxSharedStorageStringLength.Get());
}

void LogSharedStorageWorkletError(SharedStorageWorkletErrorType error_type) {
  base::UmaHistogramEnumeration("Storage.SharedStorage.Worklet.Error.Type",
                                error_type);
}

}  // namespace blink
