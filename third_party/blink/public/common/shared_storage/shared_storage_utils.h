// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_UTILS_H_

#include <cstdlib>

#include "base/strings/string_piece_forward.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

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

// Whether `privateAggregation` should be exposed to `SharedStorageWorklet`
// scope.
BLINK_COMMON_EXPORT bool ShouldDefinePrivateAggregationInSharedStorage();

// Whether the `context_id` is valid UTF-8 and has a valid length.
BLINK_COMMON_EXPORT bool IsValidPrivateAggregationContextId(
    base::StringPiece context_id);

// Maximum allowed length of the context_id string.
constexpr int kPrivateAggregationApiContextIdMaxLength = 64;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SHARED_STORAGE_SHARED_STORAGE_UTILS_H_
