// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/tensor_impl_ort.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
namespace webnn::ort {

namespace {

ONNXTensorElementDataType GetOrtDataType(OperandDataType data_type) {
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
    default:
      NOTREACHED();
  }
}

}  // namespace

TensorImplOrt::TensorImplOrt(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    ContextImplOrt* context,
    mojom::TensorInfoPtr tensor_info)
    : WebNNTensorImpl(std::move(receiver), context, std::move(tensor_info)) {
  // Convert the shape from uint32_t to int64_t.
  std::vector<int64_t> ort_shape(shape().begin(), shape().end());
  shape_ = std::move(std::move(ort_shape));

  const OrtApi* ort_api = GetOrtApi();
  OrtValue* tensor = nullptr;
  ORT_ABORT_ON_ERROR(ort_api->CreateTensorAsOrtValue(
      context->allocator(), shape_.data(), shape_.size(),
      GetOrtDataType(data_type()), &tensor));
  tensor_ = tensor;
  CHECK(tensor_);
}

TensorImplOrt::~TensorImplOrt() {
  const OrtApi* ort_api = GetOrtApi();
  ort_api->ReleaseValue(tensor_);
}

void TensorImplOrt::ReadTensorImpl(ReadTensorCallback callback) {
  static_cast<ContextImplOrt*>(context_.get())
      ->ReadTensor(this, std::move(callback));
}

void TensorImplOrt::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  static_cast<ContextImplOrt*>(context_.get())
      ->WriteTensor(this, std::move(src_buffer));
}

}  // namespace webnn::ort
