// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/tensor_impl_dml.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/context_impl_dml.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "ui/gfx/win/d3d_shared_fence.h"

namespace webnn::dml {

TensorImplDml::TensorImplDml(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info)
    : WebNNTensorImpl(std::move(receiver),
                      std::move(context),
                      std::move(tensor_info)),
      buffer_(std::move(buffer)) {}

TensorImplDml::TensorImplDml(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    RepresentationPtr representation,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info)
    : WebNNTensorImpl(std::move(receiver),
                      std::move(context),
                      std::move(tensor_info),
                      std::move(representation)),
      buffer_(representation_->GetD3D12Buffer()) {}

TensorImplDml::~TensorImplDml() = default;

void TensorImplDml::ReadTensorImpl(ReadTensorCallback callback) {
  WebNNContextImpl* context_impl = context_.get();
  CHECK(context_impl);
  static_cast<ContextImplDml*>(context_impl)
      ->ReadTensor(this, std::move(callback));
}

void TensorImplDml::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  WebNNContextImpl* context_impl = context_.get();
  CHECK(context_impl);
  static_cast<ContextImplDml*>(context_impl)
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
        command_queue->WaitForFence(wait_fence_external_->GetD3D12Fence(),
                                    wait_fence_external_->GetFenceValue()));
    wait_fence_external_.reset();
  }
  return S_OK;
}

bool TensorImplDml::BeginAccessWebNN(
    Microsoft::WRL::ComPtr<ID3D12Fence> wait_fence,
    uint64_t wait_fence_value) {
  CHECK(wait_fence);

  wait_fence_external_ = gfx::D3DSharedFence::CreateFromD3D12Fence(
      std::move(wait_fence), wait_fence_value);
  return true;
}

scoped_refptr<gfx::D3DSharedFence> TensorImplDml::EndAccessWebNN() {
  CommandQueue* command_queue =
      static_cast<ContextImplDml*>(context_.get())->GetCommandQueue();

  // If WebNN executed no commands using this tensor, the caller's command queue
  // will wait on the last wait fence provided.
  if (wait_fence_external_) {
    scoped_refptr<gfx::D3DSharedFence> fence = std::move(wait_fence_external_);
    wait_fence_external_.reset();
    return fence;
  }

  // Return WebNN's submission fence and the last fence value from execution
  // that used this tensor.
  return gfx::D3DSharedFence::CreateFromD3D12Fence(
      command_queue->submission_fence(), last_submission_fence_value_);
}

void TensorImplDml::ExportTensorImpl(ScopedAccessPtr access,
                                     ExportTensorCallback callback) {
  CHECK(access);

  auto webnn_fence_to_wait_for = EndAccessWebNN();
  CHECK(webnn_fence_to_wait_for)
      << "[WebNN] Failed to end access on WebNNTensor";

  access->SetReleaseFence(std::move(webnn_fence_to_wait_for));
  std::move(callback).Run(context_->GenVerifiedSyncToken());
}

bool TensorImplDml::ImportTensorImpl() {
  CHECK(representation_);
  // Tensor will own the access.
  auto access =
      ScopedAccessPtr(representation_->BeginScopedAccess().release(),
                      OnTaskRunnerDeleter(context_->main_task_runner()));
  if (!access) {
    return false;
  }

  // First access, no fence required.
  auto d3d_write_fence = access->GetAcquireFence();
  if (!d3d_write_fence) {
    representation_access_ = std::move(access);
    return true;
  }

  Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device;
  HRESULT hr = buffer_->GetDevice(IID_PPV_ARGS(&d3d12_device));
  CHECK_EQ(hr, S_OK) << "[WebNN] Failed to get D3D device from buffer";

  Microsoft::WRL::ComPtr<ID3D12Fence> d3d12_write_fence;
  hr = d3d12_device->OpenSharedHandle(d3d_write_fence->GetSharedHandle(),
                                      IID_PPV_ARGS(&d3d12_write_fence));
  CHECK_EQ(hr, S_OK) << "[WebNN] Failed to open handle of a D3D fence";

  if (!BeginAccessWebNN(d3d12_write_fence, d3d_write_fence->GetFenceValue())) {
    LOG(ERROR) << "[WebNN] Failed to begin access on WebNNTensor";
    return false;
  }

  representation_access_ = std::move(access);
  return true;
}

}  // namespace webnn::dml
