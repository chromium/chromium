// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_data_type.h"

#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"

namespace webnn::ort {

ONNXTensorElementDataType WebnnToOnnxDataType(OperandDataType data_type) {
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

std::optional<OperandDataType> OnnxToWebnnDataType(
    ONNXTensorElementDataType onnx_type) {
  switch (onnx_type) {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
      return OperandDataType::kFloat32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
      return OperandDataType::kFloat16;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
      return OperandDataType::kInt32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:
      return OperandDataType::kUint32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
      return OperandDataType::kInt64;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:
      return OperandDataType::kUint64;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
      return OperandDataType::kInt8;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
      return OperandDataType::kUint8;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4:
      return OperandDataType::kInt4;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4:
      return OperandDataType::kUint4;
    default:
      return std::nullopt;
  }
}

OrtHardwareDeviceType WebnnToOrtDeviceType(mojom::Device device_type) {
  switch (device_type) {
    case mojom::Device::kCpu:
      return OrtHardwareDeviceType_CPU;
    case mojom::Device::kGpu:
      return OrtHardwareDeviceType_GPU;
    case mojom::Device::kNpu:
      return OrtHardwareDeviceType_NPU;
  }
}

mojom::Device OrtToWebnnDeviceType(OrtHardwareDeviceType device_type) {
  switch (device_type) {
    case OrtHardwareDeviceType_CPU:
      return mojom::Device::kCpu;
    case OrtHardwareDeviceType_GPU:
      return mojom::Device::kGpu;
    case OrtHardwareDeviceType_NPU:
      return mojom::Device::kNpu;
  }
  NOTREACHED();
}

std::vector<int64_t> WebnnToOnnxShape(base::span<const uint32_t> shape) {
  return std::vector<int64_t>(shape.begin(), shape.end());
}

std::optional<std::vector<uint32_t>> OnnxToWebnnShape(
    base::span<const int64_t> shape) {
  std::vector<uint32_t> new_shape;
  new_shape.reserve(shape.size());
  for (int64_t dim : shape) {
    if (!base::IsValueInRangeForNumericType<uint32_t>(dim)) {
      return std::nullopt;
    }
    new_shape.push_back(static_cast<uint32_t>(dim));
  }
  return new_shape;
}

}  // namespace webnn::ort
