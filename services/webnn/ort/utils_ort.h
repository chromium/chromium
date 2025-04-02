// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_UTILS_ORT_H_
#define SERVICES_WEBNN_ORT_UTILS_ORT_H_

#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/providers/dml/dml_provider_factory.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

ONNXTensorElementDataType OperandTypeToONNXTensorElementDataType(
    OperandDataType data_type);

const OrtApi* GetOrtApi();

const OrtDmlApi* GetOrtDmlApi();

const OrtModelEditorApi* GetOrtModelEditorApi();

mojom::ErrorPtr CreateError(mojom::Error::Code error_code,
                            const std::string& error_message,
                            std::string_view label = "");

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_UTILS_ORT_H_
