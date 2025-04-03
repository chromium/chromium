// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ERROR_ORT_H_
#define SERVICES_WEBNN_ORT_ERROR_ORT_H_

#include <string>

#include "base/logging.h"
#include "base/notreached.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/ort/utils_ort.h"

namespace webnn::ort {

#define CALL_ORT_FUNC(ort_call)                                      \
  ([&]() -> ort::ScopedOrtStatus {                                   \
    ort::ScopedOrtStatus status(ort_call);                           \
    if (status.is_valid()) {                                         \
      LOG(ERROR) << "[WebNN] Failed to call " << #ort_call << ": "   \
                 << ort::GetOrtApi()->GetErrorMessage(status.get()); \
    }                                                                \
    return status;                                                   \
  })()

#define ORT_CALL_FAILED(ort_call)                         \
  ([&]() -> bool {                                        \
    ort::ScopedOrtStatus status(CALL_ORT_FUNC(ort_call)); \
    if (status.is_valid()) {                              \
      return true;                                        \
    }                                                     \
    return false;                                         \
  })()

#define RETURN_STATUS_IF_FAILED(ort_call)                 \
  do {                                                    \
    ort::ScopedOrtStatus status(CALL_ORT_FUNC(ort_call)); \
    if (status.is_valid()) {                              \
      return status;                                      \
    }                                                     \
  } while (0)

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ERROR_ORT_H_
