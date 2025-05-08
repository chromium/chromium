// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/tensor_impl_dml.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/context_impl_dml.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"

namespace webnn::dml {

SharedFence::SharedFence(Microsoft::WRL::ComPtr<ID3D12Fence> fence,
                         UINT64 fence_value)
    : fence(std::move(fence)), fence_value(fence_value) {}

SharedFence::~SharedFence() = default;

SharedFence::SharedFence(SharedFence&&) = default;

Microsoft::WRL::ComPtr<ID3D12Fence> SharedFence::GetD3D12Fence() const {
  return fence;
}

uint64_t SharedFence::GetFenceValue() const {
  return fence_value;
}

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

HRESULT TensorImplDml::WaitForExternalFenceAndReset(
    CommandQueue* command_queue) {
  if (wait_fence_external_) {
    RETURN_IF_FAILED(
        command_queue->WaitForFence(std::move(wait_fence_external_->fence),
                                    wait_fence_external_->fence_value));
    wait_fence_external_.reset();
  }
  return S_OK;
}

bool TensorImplDml::BeginAccessWebNN(
    Microsoft::WRL::ComPtr<ID3D12Fence> wait_fence,
    uint64_t wait_fence_value) {
  CHECK(wait_fence);

  wait_fence_external_.emplace(std::move(wait_fence), wait_fence_value);
  return true;
}

std::unique_ptr<native::d3d12::WebNNSharedFence>
TensorImplDml::EndAccessWebNN() {
  CommandQueue* command_queue =
      static_cast<ContextImplDml*>(context_.get())->GetCommandQueue();

  // If WebNN executed no commands using this tensor, the caller's command queue
  // will wait on the last wait fence provided.
  if (wait_fence_external_) {
    SharedFence fence = std::move(wait_fence_external_.value());
    wait_fence_external_.reset();
    return base::WrapUnique(new SharedFence(std::move(fence)));
  }

  // Return WebNN's submission fence and the last fence value from execution
  // that used this tensor.
  return base::WrapUnique(new SharedFence(
      {command_queue->submission_fence(), last_submission_fence_value_}));
}

ID3D12Resource* TensorImplDml::GetD3D12Buffer() const {
  return buffer();
}

}  // namespace webnn::dml
