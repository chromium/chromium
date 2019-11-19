// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_CONTEXT_RESULT_H_
#define GPU_COMMAND_BUFFER_COMMON_CONTEXT_RESULT_H_

#include "gpu/gpu_export.h"

namespace gpu {

// The result of trying to create a gpu context. Also the result of intermediate
// steps which bubble up to the final result. If any fatal error occurs, the
// entire result should be fatal - as any attempt to retry is expected to get
// the same fatal result.
// Note: This enum is used to back an UMA histogram. Therefore these values
// should never be reordered, renumbered, or reused.
enum class ContextResult {
  // The context was created and initialized successfully.
  kSuccess,
  // A failure occured that prevented the context from being initialized,
  // but it can be retried and expect to make progress.
  kTransientFailure,
  // An error occured that will recur in future attempts too with the
  // same inputs, retrying would not be productive.
  kFatalFailure,
  // An error occurred using the gpu::SurfaceHandle. Only retry with a new
  // SurfaceHandle; treat as kFatalFailure otherwise.
  kSurfaceFailure,
  kLastContextResult = kSurfaceFailure,
  // To use the two-arg version of the UMA_HISTOGRAM_ENUMERATION macro.
  kMaxValue = kSurfaceFailure
};

GPU_EXPORT bool IsFatalOrSurfaceFailure(ContextResult result);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_CONTEXT_RESULT_H_
