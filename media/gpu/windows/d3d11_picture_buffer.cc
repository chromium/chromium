// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_picture_buffer.h"

#include <windows.h>

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/media_log.h"
#include "media/base/win/mf_helpers.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/color_space.h"

namespace media {

D3D11PictureBuffer::D3D11PictureBuffer(
    scoped_refptr<base::SequencedTaskRunner> delete_task_runner,
    ComD3D11Texture2D texture,
    size_t array_slice,
    std::unique_ptr<Texture2DWrapper> texture_wrapper,
    gfx::Size size,
    size_t picture_index)
    : RefCountedDeleteOnSequence<D3D11PictureBuffer>(
          std::move(delete_task_runner)),
      texture_(std::move(texture)),
      array_slice_(array_slice),
      texture_wrapper_(std::move(texture_wrapper)),
      size_(size),
      picture_index_(picture_index) {}

D3D11PictureBuffer::~D3D11PictureBuffer() = default;

D3D11Status D3D11PictureBuffer::Init(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferHelperCB get_helper_cb,
    ComD3D11VideoDevice video_device,
    const GUID& decoder_guid,
    std::unique_ptr<MediaLog> media_log,
    PictureBufferGPUResourceInitDoneCB
        picture_buffer_gpu_resource_init_done_cb) {
  D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC view_desc = {};
  view_desc.DecodeProfile = decoder_guid;
  view_desc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
  view_desc.Texture2D.ArraySlice = array_slice_;

  media_log_ = std::move(media_log);
  D3D11Status result = texture_wrapper_->Init(
      std::move(gpu_task_runner), std::move(get_helper_cb), texture_,
      array_slice_, this, std::move(picture_buffer_gpu_resource_init_done_cb));
  if (!result.is_ok()) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to Initialize the wrapper";
    return result;
  }

  HRESULT hr = video_device->CreateVideoDecoderOutputView(
      Texture().Get(), &view_desc, &output_view_);

  if (!SUCCEEDED(hr)) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to CreateVideoDecoderOutputView";
    return {D3D11Status::Codes::kCreateDecoderOutputViewFailed, hr};
  }

  return D3D11Status::Codes::kOk;
}

D3D11Status D3D11PictureBuffer::ProcessTexture(
    const gfx::ColorSpace& input_color_space,
    scoped_refptr<gpu::ClientSharedImage>& shared_image_dest) {
  return texture_wrapper_->ProcessTexture(input_color_space, shared_image_dest);
}

ComD3D11Texture2D D3D11PictureBuffer::Texture() const {
  return texture_;
}

D3D11Status::Or<ID3D11VideoDecoderOutputView*>
D3D11PictureBuffer::AcquireOutputView() const {
  D3D11Status result = texture_wrapper_->BeginSharedImageAccess();
  if (!result.is_ok()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Failed to acquired key mutex for native texture resource";
    base::UmaHistogramSparse("Media.D3D11.PictureBuffer",
                             static_cast<int>(result.code()));
    return result;
  }

  return output_view_.Get();
}

D3D11Status::Or<ComD3D12Resource> D3D11PictureBuffer::ToD3D12Resource(
    ID3D12Device* device) {
  HRESULT hr;
  if (!d3d12_resource_) {
    ComDXGIResource1 dxgi_resource;
    CHECK_EQ(texture_.As(&dxgi_resource), S_OK);

    HANDLE handle;
    hr = dxgi_resource->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr,
                                           &handle);
    if (FAILED(hr)) {
      MEDIA_LOG(ERROR, media_log_) << "Cannot create shared handle";
      return {D3D11StatusCode::kCreateSharedHandleFailed, hr};
    }
    base::win::ScopedHandle handle_holder(handle);
    hr = device->OpenSharedHandle(handle_holder.get(),
                                  IID_PPV_ARGS(&d3d12_resource_));
    if (FAILED(hr)) {
      LOG(ERROR) << "Open shared handle as D3D12 resource failed.";
      return {D3D11StatusCode::kCreateSharedHandleFailed, hr};
    }
  }
  ComD3D12Device used_device;
  hr = d3d12_resource_->GetDevice(IID_PPV_ARGS(&used_device));
  if (FAILED(hr)) {
    LOG(ERROR) << "ID3D12Resource::GetDevice failed.";
    return {D3D11StatusCode::kGetDeviceFailed, hr};
  }
  CHECK_EQ(used_device.Get(), device);
  return d3d12_resource_;
}

}  // namespace media
