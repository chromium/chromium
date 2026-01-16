// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/tensor_impl_ort.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "ui/gfx/win/d3d_shared_fence.h"

namespace webnn::ort {

TensorImplOrt::TensorImplOrt(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info,
    size_t size,
    ScopedOrtValue tensor,
    bool can_access_on_cpu,
    scoped_refptr<DeviceAllocator> device_allocator)
    : WebNNTensorImpl(std::move(receiver),
                      std::move(context),
                      std::move(tensor_info)),
      device_allocator_((std::move(device_allocator))),
      tensor_(std::move(tensor)),
      size_(size) {
  // Initialize the tensor with zeros, otherwise, reading uninitialized memory
  // will get random values.
  // TODO(crbug.com/461303833): check whether fast HW clears can be used
  // instead.
  if (can_access_on_cpu) {
    std::ranges::fill(AsSpan(), 0);
  }
}

TensorImplOrt::TensorImplOrt(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info,
    RepresentationPtr representation,
    size_t size,
    ScopedOrtValue tensor)
    : WebNNTensorImpl(std::move(receiver),
                      std::move(context),
                      std::move(tensor_info),
                      std::move(representation)),
      tensor_(std::move(tensor)),
      size_(size) {}

TensorImplOrt::~TensorImplOrt() = default;

base::span<uint8_t> TensorImplOrt::AsSpan() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  void* ort_tensor_raw_data = nullptr;
  CHECK_STATUS(
      PlatformFunctions::GetInstance()->ort_api()->GetTensorMutableData(
          tensor_.get(), &ort_tensor_raw_data));
  CHECK(ort_tensor_raw_data);
  // SAFETY: ORT guarantees that it has allocated enough memory to
  // store tensor.
  return UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(ort_tensor_raw_data), size_));
}

void TensorImplOrt::ReadTensorImpl(ReadTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  base::span<const uint8_t> buffer_span = AsSpan();
  CHECK_EQ(PackedByteLength(), buffer_span.size());
  std::move(callback).Run(mojom::ReadTensorResult::NewBuffer(
      context_->WriteDataToDataPipeOrBigBuffer(buffer_span)));
}

void TensorImplOrt::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  context_->ReadDataFromBigBufferOrDataPipe(std::move(src_buffer), AsSpan());
}

bool TensorImplOrt::ImportTensorImpl(ScopedAccessPtr access) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  // No synchronization needed if there is no fence to acquire.
  scoped_refptr<gfx::D3DSharedFence> d3d_write_fence =
      access->GetAcquireFence();
  if (!d3d_write_fence) {
    representation_access_ = std::move(access);
    return true;
  }

  Microsoft::WRL::ComPtr<ID3D12Fence> d3d12_wait_fence =
      d3d_write_fence->GetD3D12Fence();
  CHECK(d3d12_wait_fence)
      << "[WebNN] Failed to get D3D12 fence from shared fence";

  if (d3d12_wait_fence->GetCompletedValue() <
      d3d_write_fence->GetFenceValue()) {
    // Passing nullptr as the event handle to SetEventOnCompletion means the
    // function will block until the fence is signaled.
    HRESULT hr = d3d12_wait_fence->SetEventOnCompletion(
        d3d_write_fence->GetFenceValue(), nullptr);
    if (FAILED(hr)) {
      LOG(ERROR) << "[WebNN] Failed to set event on completion: "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }
  }

  representation_access_ = std::move(access);
  return true;
}

void TensorImplOrt::ExportTensorImpl(ScopedAccessPtr access,
                                     ExportTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  // Since we wait for all WebNN operations to complete, we only need to release
  // the ScopedAccess to end WebNN access.

  std::move(callback).Run(context_->GenVerifiedSyncToken());
}

}  // namespace webnn::ort
