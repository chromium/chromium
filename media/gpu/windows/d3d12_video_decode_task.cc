// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_decode_task.h"

#include "base/task/thread_pool.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_core.h"

namespace media {

D3D12VideoDecoderTask::D3D12VideoDecoderTask() = default;

D3D12VideoDecoderTask::~D3D12VideoDecoderTask() {
  WaitForCompletion();
}

void D3D12VideoDecoderTask::SetFenceAndValue(scoped_refptr<D3D12Fence> fence,
                                             uint64_t value) {
  CHECK(!fence_and_value_.first);
  fence_and_value_ = {std::move(fence), value};
}

bool D3D12VideoDecoderTask::WaitForCompletion() {
  const auto& [fence, value] = fence_and_value_;
  if (fence) {
    bool ok = fence->WaitCPU(value) == D3D11StatusCode::kOk;
    if (ok) {
      fence_and_value_ = {nullptr, 0};
    }
    return ok;
  }
  return true;
}

void D3D12VideoDecoderTask::ResetBuffers() {
  CHECK(!fence_and_value_.first);
  picture_parameters_buffer_.clear();
  inverse_quantization_matrix_buffer_.clear();
  slice_control_buffer_.clear();
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator>
D3D12VideoDecoderTask::ResetAndGetCommandAllocator(ID3D12Device* device) {
  CHECK(!fence_and_value_.first);

  HRESULT hr;
  if (!command_allocator_) {
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE,
                                        IID_PPV_ARGS(&command_allocator_));
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to create command allocator" << PrintHr(hr);
      return nullptr;
    }
  } else {
    hr = command_allocator_->Reset();
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to reset command allocator" << PrintHr(hr);
      return nullptr;
    }
  }
  return command_allocator_;
}

std::vector<uint8_t>& D3D12VideoDecoderTask::GetPictureParametersBuffer() {
  CHECK(!fence_and_value_.first);
  return picture_parameters_buffer_;
}

std::vector<uint8_t>&
D3D12VideoDecoderTask::GetInverseQuantizationMatrixBuffer() {
  CHECK(!fence_and_value_.first);
  return inverse_quantization_matrix_buffer_;
}

std::vector<uint8_t>& D3D12VideoDecoderTask::GetSliceControlBuffer() {
  CHECK(!fence_and_value_.first);
  return slice_control_buffer_;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
D3D12VideoDecoderTask::GetBitstreamBuffer(ID3D12Device* device, size_t size) {
  CHECK(!fence_and_value_.first);
  // The compressed_bitstream_ is reused if it's large enough.
  if (!compressed_bitstream_ || compressed_bitstream_->GetDesc().Width < size) {
    // Double the size of the buffer if it's too small, to avoid frequent
    // reallocations.
    size_t new_size = std::max(
        compressed_bitstream_
            ? static_cast<size_t>(compressed_bitstream_->GetDesc().Width) * 2
            : size,
        size);
    D3D12_HEAP_PROPERTIES heap_properties{.Type = D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(new_size);
    HRESULT hr = device->CreateCommittedResource(
        &heap_properties, {}, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&compressed_bitstream_));
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to create committed resource" << PrintHr(hr);
      return nullptr;
    }
  }
  return compressed_bitstream_;
}

}  // namespace media
