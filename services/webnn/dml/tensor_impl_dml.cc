// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/tensor_impl_dml.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/dml/context_impl_dml.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"

namespace webnn::dml {

TensorImplDml::TensorImplDml(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer,
    ContextImplDml* context,
    mojom::TensorInfoPtr tensor_info)
    : WebNNTensorImpl(std::move(receiver), context, std::move(tensor_info)),
      buffer_(std::move(buffer)) {}

TensorImplDml::~TensorImplDml() = default;

void TensorImplDml::ReadTensorImpl(ReadTensorCallback callback) {
  static_cast<ContextImplDml*>(context_.get())
      ->ReadTensor(this, std::move(callback));
}

void TensorImplDml::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  static_cast<ContextImplDml*>(context_.get())
      ->WriteTensor(this, std::move(src_buffer));
}

void TensorImplDml::SetLastSubmissionFenceValue(
    uint64_t last_submission_fence_value) {
  last_submission_fence_value_ = last_submission_fence_value;
}

uint64_t TensorImplDml::last_submission_fence_value() const {
  return last_submission_fence_value_;
}

}  // namespace webnn::dml
