// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ORT_DATA_TYPE_H_
#define SERVICES_WEBNN_ORT_ORT_DATA_TYPE_H_

#include "services/webnn/public/cpp/operand_descriptor.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

ONNXTensorElementDataType WebnnToOnnxDataType(OperandDataType data_type);

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ORT_DATA_TYPE_H_
