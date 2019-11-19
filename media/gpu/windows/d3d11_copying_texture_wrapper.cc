// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"

#include <memory>

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "media/gpu/windows/d3d11_com_defs.h"

namespace media {

// TODO(tmathmeyer) What D3D11 Resources do we need to do the copying?
CopyingTexture2DWrapper::CopyingTexture2DWrapper(
    std::unique_ptr<Texture2DWrapper> output_wrapper,
    std::unique_ptr<VideoProcessorProxy> processor,
    ComD3D11Texture2D input_texture)
    : Texture2DWrapper(input_texture),
      video_processor_(std::move(processor)),
      output_texture_wrapper_(std::move(output_wrapper)) {}

CopyingTexture2DWrapper::~CopyingTexture2DWrapper() = default;

#define RETURN_ON_FAILURE(expr) \
  do {                          \
    if (!SUCCEEDED((expr))) {   \
      return false;             \
    }                           \
  } while (0)

bool CopyingTexture2DWrapper::ProcessTexture(const D3D11PictureBuffer* owner_pb,
                                             MailboxHolderArray* mailbox_dest) {
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {
      D3D11_VPOV_DIMENSION_TEXTURE2D};
  output_view_desc.Texture2D.MipSlice = 0;
  ComD3D11VideoProcessorOutputView output_view;
  RETURN_ON_FAILURE(video_processor_->CreateVideoProcessorOutputView(
      output_texture_wrapper_->Texture().Get(), &output_view_desc,
      &output_view));

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {0};
  input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  input_view_desc.Texture2D.ArraySlice = owner_pb->level();
  input_view_desc.Texture2D.MipSlice = 0;
  ComD3D11VideoProcessorInputView input_view;
  RETURN_ON_FAILURE(video_processor_->CreateVideoProcessorInputView(
      Texture().Get(), &input_view_desc, &input_view));

  D3D11_VIDEO_PROCESSOR_STREAM streams = {0};
  streams.Enable = TRUE;
  streams.pInputSurface = input_view.Get();

  RETURN_ON_FAILURE(video_processor_->VideoProcessorBlt(output_view.Get(),
                                                        0,  // output_frameno
                                                        1,  // stream_count
                                                        &streams));

  return output_texture_wrapper_->ProcessTexture(owner_pb, mailbox_dest);
}

bool CopyingTexture2DWrapper::Init(GetCommandBufferHelperCB get_helper_cb,
                                   size_t array_slice,
                                   gfx::Size size) {
  if (!video_processor_->Init(size.width(), size.height()))
    return false;

  return output_texture_wrapper_->Init(
      get_helper_cb,
      0,  // The output texture only has an array size of 1.
      size);
}

}  // namespace media
