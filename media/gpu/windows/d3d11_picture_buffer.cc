// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_picture_buffer.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <windows.h>
#include <wrl/client.h>

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
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

D3D11PictureBuffer::~D3D11PictureBuffer() {
}

D3D11Status D3D11PictureBuffer::Init(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferHelperCB get_helper_cb,
    ComD3D11VideoDevice video_device,
    const GUID& decoder_guid,
    std::unique_ptr<MediaLog> media_log) {
  D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC view_desc = {};
  view_desc.DecodeProfile = decoder_guid;
  view_desc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
  view_desc.Texture2D.ArraySlice = array_slice_;

  media_log_ = std::move(media_log);
  D3D11Status result =
      texture_wrapper_->Init(std::move(gpu_task_runner),
                             std::move(get_helper_cb), texture_, array_slice_);
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
    MailboxHolderArray* mailbox_dest,
    gfx::ColorSpace* output_color_space) {
  return texture_wrapper_->ProcessTexture(input_color_space, mailbox_dest,
                                          output_color_space);
}

ComD3D11Texture2D D3D11PictureBuffer::Texture() const {
  return texture_;
}

D3D11Status::Or<ID3D11VideoDecoderOutputView*>
D3D11PictureBuffer::AcquireOutputView() const {
  D3D11Status result = texture_wrapper_->AcquireKeyedMutexIfNeeded();
  if (!result.is_ok()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Failed to acquired key mutex for native texture resource";
    base::UmaHistogramSparse("Media.D3D11.PictureBuffer",
                             static_cast<int>(result.code()));
    return result;
  }

  return output_view_.Get();
}

}  // namespace media
