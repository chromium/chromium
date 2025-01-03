// Copyright 2024 The Chromium Authors
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
    const OrtApi* ort_api_local = GetOrtApi();                       \
    OrtStatus* onnx_status = (expr);                                 \
    if (onnx_status != NULL) {                                       \
      std::string msg = ort_api_local->GetErrorMessage(onnx_status); \
      ort_api_local->ReleaseStatus(onnx_status);                     \
      NOTREACHED() << "[WebNN] Ort Status: " << msg;                 \
    }                                                                \
  } while (0);

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ERROR_ORT_H_
