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
      OperandTypeToONNXTensorElementDataType(data_type()), &tensor));
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
