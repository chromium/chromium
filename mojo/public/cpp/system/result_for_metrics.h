// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_RESULT_FOR_METRICS_H_
#define MOJO_PUBLIC_CPP_SYSTEM_RESULT_FOR_METRICS_H_

#include "mojo/public/c/system/types.h"

namespace mojo {

// An enum for tracking `MojoResult` codes in metrics. `MojoResult` itself is a
// plain `uint32_t` for C compatibility, which is not ideal for UMA histograms
// that expect a proper enum. This scoped enum provides type safety for metrics.
//
// This should be kept in sync with the MojoResult definitions in types.h.
//
// LINT.IfChange(MojoResultForMetrics)
enum class MojoResultForMetrics {
  kOk = MOJO_RESULT_OK,
  kCancelled = MOJO_RESULT_CANCELLED,
  kUnknown = MOJO_RESULT_UNKNOWN,
  kInvalidArgument = MOJO_RESULT_INVALID_ARGUMENT,
  kDeadlineExceeded = MOJO_RESULT_DEADLINE_EXCEEDED,
  kNotFound = MOJO_RESULT_NOT_FOUND,
  kAlreadyExists = MOJO_RESULT_ALREADY_EXISTS,
  kPermissionDenied = MOJO_RESULT_PERMISSION_DENIED,
  kResourceExhausted = MOJO_RESULT_RESOURCE_EXHAUSTED,
  kFailedPrecondition = MOJO_RESULT_FAILED_PRECONDITION,
  kAborted = MOJO_RESULT_ABORTED,
  kOutOfRange = MOJO_RESULT_OUT_OF_RANGE,
  kUnimplemented = MOJO_RESULT_UNIMPLEMENTED,
  kInternal = MOJO_RESULT_INTERNAL,
  kUnavailable = MOJO_RESULT_UNAVAILABLE,
  kDataLoss = MOJO_RESULT_DATA_LOSS,
  kBusy = MOJO_RESULT_BUSY,
  kShouldWait = MOJO_RESULT_SHOULD_WAIT,

  // This is required by UMA histogram macros.
  kMaxValue = kShouldWait,
};

// Converts a MojoResult to the MojoResultForMetrics enum for use in histograms.
inline MojoResultForMetrics MojoResultToMetricsEnum(MojoResult result) {
  if (result >= MOJO_RESULT_OK && result <= MOJO_RESULT_SHOULD_WAIT) {
    return static_cast<MojoResultForMetrics>(result);
  }
  // Map any unknown/new error codes to a known bucket.
  return MojoResultForMetrics::kUnknown;
}
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:MojoResult)

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_RESULT_FOR_METRICS_H_