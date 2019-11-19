// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_picture_buffer.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <windows.h>
#include <wrl/client.h>

#include <memory>

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/media_log.h"
#include "media/gpu/windows/return_on_failure.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/color_space.h"

namespace media {

D3D11PictureBuffer::D3D11PictureBuffer(
    std::unique_ptr<Texture2DWrapper> texture_wrapper,
    gfx::Size size,
    size_t level)
    : texture_wrapper_(std::move(texture_wrapper)),
      size_(size),
      level_(level) {}

D3D11PictureBuffer::~D3D11PictureBuffer() {
  // TODO(liberato): post destruction of |gpu_resources_| to the gpu thread.
}

bool D3D11PictureBuffer::Init(GetCommandBufferHelperCB get_helper_cb,
                              ComD3D11VideoDevice video_device,
                              const GUID& decoder_guid,
                              std::unique_ptr<MediaLog> media_log) {
  D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC view_desc = {};
  view_desc.DecodeProfile = decoder_guid;
  view_desc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
  view_desc.Texture2D.ArraySlice = (UINT)level_;

  if (!texture_wrapper_->Init(std::move(get_helper_cb), level_, size_)) {
    media_log->AddEvent(
        media_log->CreateStringEvent(MediaLogEvent::MEDIA_ERROR_LOG_ENTRY,
                                     "error", "Failed to Init the wrapper"));
    return false;
  }

  HRESULT hr = video_device->CreateVideoDecoderOutputView(
      Texture().Get(), &view_desc, &output_view_);

  if (!SUCCEEDED(hr)) {
    media_log->AddEvent(media_log->CreateStringEvent(
        MediaLogEvent::MEDIA_ERROR_LOG_ENTRY, "error",
        "Failed to CreateVideoDecoderOutputView"));
    return false;
  }

  return true;
}

bool D3D11PictureBuffer::ProcessTexture(MailboxHolderArray* mailbox_dest) {
  return texture_wrapper_->ProcessTexture(this, mailbox_dest);
}

ComD3D11Texture2D D3D11PictureBuffer::Texture() const {
  return texture_wrapper_->Texture();
}

}  // namespace media
