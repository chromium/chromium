// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d_picture_buffer.h"

#include <windows.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/media_log.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/d3d12_video_decode_task.h"
#include "ui/gfx/color_space.h"

namespace media {

D3DPictureBuffer::D3DPictureBuffer(
    scoped_refptr<base::SequencedTaskRunner> delete_task_runner,
    size_t array_slice,
    std::unique_ptr<D3DTextureWrapper> texture_wrapper,
    const gfx::Size& size,
    size_t picture_index)
    : RefCountedDeleteOnSequence<D3DPictureBuffer>(
          std::move(delete_task_runner)),
      array_slice_(array_slice),
      size_(size),
      texture_wrapper_(std::move(texture_wrapper)),
      picture_index_(picture_index) {}

D3DPictureBuffer::~D3DPictureBuffer() = default;

D3DStatus D3DPictureBuffer::BeginSharedImageAccess() const {
  D3DStatus result = texture_wrapper_->BeginSharedImageAccess();
  if (!result.is_ok()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Failed to acquired key mutex for native texture resource";
    base::UmaHistogramSparse("Media.D3D11.PictureBuffer",
                             static_cast<int>(result.code()));
  }

  return result;
}

D3DStatus D3DPictureBuffer::ProcessTexture(
    scoped_refptr<gpu::ClientSharedImage>& shared_image_dest) {
  return texture_wrapper_->ProcessTexture(shared_image_dest,
                                          shared_decode_fence_);
}

void D3DPictureBuffer::SetFenceAndValue(scoped_refptr<D3D12Fence> fence,
                                        uint64_t value) {
  CHECK(fence);
  if (!shared_decode_fence_) {
    shared_decode_fence_ = fence->CreateSharedFence(value);
  } else {
    CHECK_EQ(shared_decode_fence_->GetD3D12Fence().Get(), fence->Get());
    CHECK_GE(value, shared_decode_fence_->GetFenceValue());
    shared_decode_fence_->Update(value);
  }
}

D3D11PictureBufferImpl::D3D11PictureBufferImpl(
    scoped_refptr<base::SequencedTaskRunner> delete_task_runner,
    ComD3D11Texture2D texture,
    size_t array_slice,
    std::unique_ptr<D3DTextureWrapper> texture_wrapper,
    const gfx::Size& size,
    size_t picture_index)
    : D3DPictureBuffer(std::move(delete_task_runner),
                       array_slice,
                       std::move(texture_wrapper),
                       size,
                       picture_index),
      d3d11_texture_(std::move(texture)) {
  CHECK(d3d11_texture_);
}

D3D11PictureBufferImpl::~D3D11PictureBufferImpl() = default;

D3DStatus D3D11PictureBufferImpl::InitD3D11(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferHelperCB get_helper_cb,
    ComD3D11VideoDevice1 video_device,
    const GUID& decoder_guid,
    std::unique_ptr<MediaLog> media_log,
    PictureBufferGPUResourceInitDoneCB
        picture_buffer_gpu_resource_init_done_cb) {
  CHECK(d3d11_texture_);

  D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC view_desc = {};
  view_desc.DecodeProfile = decoder_guid;
  view_desc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
  view_desc.Texture2D.ArraySlice = array_slice();

  media_log_ = std::move(media_log);
  D3DStatus result = texture_wrapper()->Init(
      std::move(gpu_task_runner), std::move(get_helper_cb), d3d11_texture_,
      array_slice(), this, std::move(picture_buffer_gpu_resource_init_done_cb));
  if (!result.is_ok()) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to Initialize the wrapper";
    return result;
  }

  HRESULT hr = video_device->CreateVideoDecoderOutputView(
      d3d11_texture_.Get(), &view_desc, &output_view_);
  if (FAILED(hr)) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to CreateVideoDecoderOutputView";
    return {D3DStatus::Codes::kCreateDecoderOutputViewFailed, hr};
  }

  return D3DStatus::Codes::kOk;
}

base::expected<ID3D11VideoDecoderOutputView*, D3DStatus>
D3D11PictureBufferImpl::AcquireOutputView() const {
  D3DStatus result = BeginSharedImageAccess();
  if (!result.is_ok()) {
    return base::unexpected(std::move(result));
  }

  return output_view_.Get();
}

base::expected<ID3D12Resource*, D3DStatus>
D3D11PictureBufferImpl::GetOrCreateD3D12Resource(ID3D12Device* device) {
  if (!d3d12_resource_) {
    CHECK(d3d11_texture_);
    D3D11_TEXTURE2D_DESC desc;
    d3d11_texture_->GetDesc(&desc);
    CHECK(desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE)
        << "Cannot create shared handle for d3d11 texture2d without "
           "D3D11_RESOURCE_MISC_SHARED_NTHANDLE flag.";

    ComDXGIResource1 dxgi_resource;
    CHECK_EQ(d3d11_texture_.As(&dxgi_resource), S_OK);

    HANDLE handle;
    HRESULT hr = dxgi_resource->CreateSharedHandle(nullptr, GENERIC_ALL,
                                                   nullptr, &handle);
    if (FAILED(hr)) {
      MEDIA_LOG(ERROR, media_log_) << "Cannot create shared handle";
      return base::unexpected(
          D3DStatus{D3DStatusCode::kCreateSharedHandleFailed, hr});
    }
    base::win::ScopedHandle handle_holder(handle);
    hr = device->OpenSharedHandle(handle_holder.get(),
                                  IID_PPV_ARGS(&d3d12_resource_));
    if (FAILED(hr)) {
      MEDIA_LOG(ERROR, media_log_)
          << "Open shared handle as D3D12 resource failed.";
      return base::unexpected(
          D3DStatus{D3DStatusCode::kCreateSharedHandleFailed, hr});
    }
  }
#if DCHECK_IS_ON()
  ComD3D12Device used_device;
  CHECK_EQ(d3d12_resource_->GetDevice(IID_PPV_ARGS(&used_device)), S_OK);
  DCHECK_EQ(used_device.Get(), device);
#endif

  return d3d12_resource_.Get();
}

D3DPictureBuffer::Type D3D11PictureBufferImpl::GetType() const {
  return kType;
}

D3D12PictureBufferImpl::D3D12PictureBufferImpl(
    scoped_refptr<base::SequencedTaskRunner> delete_task_runner,
    ComD3D12Resource resource,
    uint32_t array_slice,
    std::unique_ptr<D3DTextureWrapper> texture_wrapper,
    const gfx::Size& size,
    size_t picture_index)
    : D3DPictureBuffer(std::move(delete_task_runner),
                       array_slice,
                       std::move(texture_wrapper),
                       size,
                       picture_index),
      d3d12_resource_(std::move(resource)) {
  CHECK(d3d12_resource_);
}

D3D12PictureBufferImpl::~D3D12PictureBufferImpl() = default;

base::expected<ID3D12Resource*, D3DStatus>
D3D12PictureBufferImpl::GetOrCreateD3D12Resource(ID3D12Device* device) {
  return d3d12_resource_.Get();
}

D3DPictureBuffer::Type D3D12PictureBufferImpl::GetType() const {
  return kType;
}

}  // namespace media
