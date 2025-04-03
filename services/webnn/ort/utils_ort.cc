// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/utils_ort.h"

#include "base/strings/strcat.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/webnn_errors.h"

namespace webnn::ort {

namespace {

const char kBackendName[] = "Ort: ";

}  // namespace

ONNXTensorElementDataType OperandTypeToONNXTensorElementDataType(
    OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
    case OperandDataType::kFloat16:
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16;
    case OperandDataType::kInt32:
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
    case OperandDataType::kUint32:
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32;
    case OperandDataType::kInt64:
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
    case OperandDataType::kUint64:
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64;
    case OperandDataType::kInt8:
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8;
    case OperandDataType::kUint8:
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8;
    case OperandDataType::kInt4:
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4;
    case OperandDataType::kUint4:
      return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4;
  }
}

const OrtApi* GetOrtApi() {
  PlatformFunctions* platform_functions = PlatformFunctions::GetInstance();
  CHECK(platform_functions);
  return platform_functions->ort_api();
}

const OrtDmlApi* GetOrtDmlApi() {
  PlatformFunctions* platform_functions = PlatformFunctions::GetInstance();
  CHECK(platform_functions);
  return platform_functions->ort_dml_api();
}

const OrtModelEditorApi* GetOrtModelEditorApi() {
  PlatformFunctions* platform_functions = PlatformFunctions::GetInstance();
  CHECK(platform_functions);
  return platform_functions->ort_model_editor_api();
}

mojom::ErrorPtr CreateError(mojom::Error::Code error_code,
                            const std::string& error_message,
                            std::string_view label) {
  LOG(ERROR) << "[WebNN] CreateError: " << error_message;
  if (!label.empty()) {
    return mojom::Error::New(
        error_code, base::StrCat({kBackendName, GetErrorLabelPrefix(label),
                                  error_message}));
  }
  return mojom::Error::New(error_code,
                           base::StrCat({kBackendName, error_message}));
}

bool IsSuccess(OrtStatus* status) {
  if (status == NULL) {
    return true;
  }
  LOG(ERROR) << "[WebNN] ORT status error code: "
             << GetOrtApi()->GetErrorCode(status)
             << " error message: " << GetOrtApi()->GetErrorMessage(status);
  GetOrtApi()->ReleaseStatus(status);
  return false;
}

}  // namespace webnn::ort
