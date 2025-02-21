// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ERROR_ORT_H_
#define SERVICES_WEBNN_ORT_ERROR_ORT_H_

#include <string>

#include "base/logging.h"
#include "base/notreached.h"
#include "services/webnn/ort/utils_ort.h"

namespace webnn::ort {

#define CHECK_STATUS(expr)                                           \
  do {                                                               \
    const OrtApi* ort_api_local = ort::GetOrtApi();                  \
    OrtStatus* onnx_status = (expr);                                 \
    if (onnx_status != NULL) {                                       \
      std::string msg = ort_api_local->GetErrorMessage(onnx_status); \
      ort_api_local->ReleaseStatus(onnx_status);                     \
      NOTREACHED() << "[WebNN] Ort Status: " << msg;                 \
    }                                                                \
  } while (0);

#define CALL_ORT_FUNC(ort_call)                                    \
  ([&]() -> ort::ScopedOrtStatusPtr {                              \
    ort::ScopedOrtStatusPtr status(ort_call);                      \
    if (status) {                                                  \
      LOG(ERROR) << "[WebNN] Failed to call " << #ort_call << ": " \
                 << ort::GetOrtApi()->GetErrorMessage(status);     \
    }                                                              \
    return status;                                                 \
  })()

#define ORT_CALL_FAILED(ort_call)                            \
  ([&]() -> bool {                                           \
    ort::ScopedOrtStatusPtr status(CALL_ORT_FUNC(ort_call)); \
    if (status) {                                            \
      return true;                                           \
    }                                                        \
    return false;                                            \
  })()

#define RETURN_STATUS_IF_FAILED(ort_call)                    \
  do {                                                       \
    ort::ScopedOrtStatusPtr status(CALL_ORT_FUNC(ort_call)); \
    if (status) {                                            \
      return status;                                         \
    }                                                        \
  } while (0)

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ERROR_ORT_H_
