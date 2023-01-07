// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_UTILS_H_

#include <cstdlib>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Allows the `OnVoidOperationFinished()` callbacks in both the shared storage
// worklet and the shared storage window to pass a parameter identifying the
// calling operation, for the purpose of recording timing information to the
// correct histogram. Also used for logging any error in the case of `run()`.
enum class SharedStorageVoidOperation {
  kRun = 0,
  kSet = 1,
  kAppend = 2,
  kDelete = 3,
  kClear = 4,
};

// Whether or not the worklet ever entered keep-alive, and if so, the reason the
// keep-alive was terminated. Recorded to UMA; always add new values to the end
// and do not reorder or delete values from this list.
enum class SharedStorageWorkletDestroyedStatus {
  kDidNotEnterKeepAlive = 0,
  kKeepAliveEndedDueToOperationsFinished = 1,
  kKeepAliveEndedDueToTimeout = 2,
  kOther = 3,

  // Keep this at the end and equal to the last entry.
  kMaxValue = kOther,
};

// Error type encountered by worklet.
// Recorded to UMA; always add new values to the end and do not reorder or
// delete values from this list.
enum class SharedStorageWorkletErrorType {
  kAddModuleWebVisible = 0,
  kAddModuleNonWebVisible = 1,
  kRunWebVisible = 2,
  kRunNonWebVisible = 3,
  kSelectURLWebVisible = 4,
  kSelectURLNonWebVisible = 5,

  // Keep this at the end and equal to the last entry.
  kMaxValue = kSelectURLNonWebVisible,
};

// Whether the length of the urls input parameter (of the
// sharedStorage.runURLSelectionOperation method) is valid.
BLINK_COMMON_EXPORT bool IsValidSharedStorageURLsArrayLength(size_t length);

// Whether the length of a shared storage's key is valid.
BLINK_COMMON_EXPORT bool IsValidSharedStorageKeyStringLength(size_t length);

// Whether the length of shared storage's value is valid.
BLINK_COMMON_EXPORT bool IsValidSharedStorageValueStringLength(size_t length);

// Logs histograms of the calling method and error type for worklet errors.
BLINK_COMMON_EXPORT void LogSharedStorageWorkletError(
    SharedStorageWorkletErrorType error_type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_UTILS_H_
