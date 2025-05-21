// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ORT_STATUS_H_
#define SERVICES_WEBNN_ORT_ORT_STATUS_H_

#include <string>

#include "base/logging.h"
#include "services/webnn/ort/scoped_ort_types.h"

struct OrtStatus;

namespace webnn::ort {

namespace internal {

std::string OrtStatusErrorMessage(OrtStatus* status);

}  // namespace internal

#define CHECK_STATUS(expr)                                      \
  if (OrtStatus* status = (expr)) {                             \
    LOG(FATAL) << ort::internal::OrtStatusErrorMessage(status); \
  }

#define CALL_ORT_FUNC(expr)                                             \
  ([&]() -> ort::ScopedOrtStatus {                                      \
    ort::ScopedOrtStatus status(expr);                                  \
    if (status.is_valid()) {                                            \
      LOG(ERROR) << "[WebNN] Failed to call " << #expr << ": "          \
                 << ort::internal::OrtStatusErrorMessage(status.get()); \
    }                                                                   \
    return status;                                                      \
  })()

#define ORT_CALL_FAILED(expr)                         \
  ([&]() -> bool {                                    \
    ort::ScopedOrtStatus status(CALL_ORT_FUNC(expr)); \
    if (status.is_valid()) {                          \
      return true;                                    \
    }                                                 \
    return false;                                     \
  })()

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ORT_STATUS_H_
