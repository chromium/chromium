// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"

#include <memory>

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "media/base/status_codes.h"
#include "media/base/win/hresult_status_helper.h"
#include "media/gpu/windows/d3d11_com_defs.h"
#include "ui/gl/hdr_metadata_helper_win.h"

namespace media {

// TODO(tmathmeyer) What D3D11 Resources do we need to do the copying?
CopyingTexture2DWrapper::CopyingTexture2DWrapper(
    const gfx::Size& size,
    std::unique_ptr<Texture2DWrapper> output_wrapper,
    std::unique_ptr<VideoProcessorProxy> processor,
    ComD3D11Texture2D output_texture,
    base::Optional<gfx::ColorSpace> output_color_space)
    : size_(size),
      video_processor_(std::move(processor)),
      output_texture_wrapper_(std::move(output_wrapper)),
      output_texture_(std::move(output_texture)),
      output_color_space_(std::move(output_color_space)) {}

CopyingTexture2DWrapper::~CopyingTexture2DWrapper() = default;

Status CopyingTexture2DWrapper::ProcessTexture(
    const gfx::ColorSpace& input_color_space,
    MailboxHolderArray* mailbox_dest,
    gfx::ColorSpace* output_color_space) {
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {
      D3D11_VPOV_DIMENSION_TEXTURE2D};
  output_view_desc.Texture2D.MipSlice = 0;
  ComD3D11VideoProcessorOutputView output_view;
  HRESULT hr = video_processor_->CreateVideoProcessorOutputView(
      output_texture_.Get(), &output_view_desc, &output_view);
  if (!SUCCEEDED(hr)) {
    return Status(StatusCode::kCreateVideoProcessorOutputViewFailed)
        .AddCause(HresultToStatus(hr));
  }

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {0};
  input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  input_view_desc.Texture2D.ArraySlice = array_slice_;
  input_view_desc.Texture2D.MipSlice = 0;
  ComD3D11VideoProcessorInputView input_view;
  hr = video_processor_->CreateVideoProcessorInputView(
      texture_.Get(), &input_view_desc, &input_view);
  if (!SUCCEEDED(hr)) {
    return Status(StatusCode::kCreateVideoProcessorInputViewFailed)
        .AddCause(HresultToStatus(hr));
  }

  D3D11_VIDEO_PROCESSOR_STREAM streams = {0};
  streams.Enable = TRUE;
  streams.pInputSurface = input_view.Get();

  // If we were given an output color space, then that's what we'll use.
  // Otherwise, we'll use whatever the input space is.
  gfx::ColorSpace copy_color_space =
      output_color_space_ ? *output_color_space_ : input_color_space;

  // If the input color space has changed, or if this is the first call, then
  // notify the video processor about it.
  if (!previous_input_color_space_ ||
      *previous_input_color_space_ != input_color_space) {
    previous_input_color_space_ = input_color_space;
    video_processor_->SetStreamColorSpace(input_color_space);
    video_processor_->SetOutputColorSpace(copy_color_space);
  }

  hr = video_processor_->VideoProcessorBlt(output_view.Get(),
                                           0,  // output_frameno
                                           1,  // stream_count
                                           &streams);
  if (!SUCCEEDED(hr)) {
    return Status(StatusCode::kVideoProcessorBltFailed)
        .AddCause(HresultToStatus(hr));
  }

  return output_texture_wrapper_->ProcessTexture(copy_color_space, mailbox_dest,
                                                 output_color_space);
}

Status CopyingTexture2DWrapper::Init(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferHelperCB get_helper_cb,
    ComD3D11Texture2D texture,
    size_t array_slice) {
  auto result = video_processor_->Init(size_.width(), size_.height());
  if (!result.is_ok())
    return std::move(result).AddHere();

  // Remember the texture + array_slice so later, ProcessTexture can still use
  // it.
  texture_ = texture;
  array_slice_ = array_slice;

  return output_texture_wrapper_->Init(std::move(gpu_task_runner),
                                       std::move(get_helper_cb),
                                       output_texture_, /*array_slice=*/0);
}

void CopyingTexture2DWrapper::SetStreamHDRMetadata(
    const gl::HDRMetadata& stream_metadata) {
  auto dxgi_stream_metadata =
      gl::HDRMetadataHelperWin::HDRMetadataToDXGI(stream_metadata);
  video_processor_->SetStreamHDRMetadata(dxgi_stream_metadata);
}

void CopyingTexture2DWrapper::SetDisplayHDRMetadata(
    const DXGI_HDR_METADATA_HDR10& dxgi_display_metadata) {
  video_processor_->SetDisplayHDRMetadata(dxgi_display_metadata);
}

}  // namespace media
