// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"

#include "media/gpu/windows/d3d11_picture_buffer.h"

namespace media {

// TODO(tmathmeyer) What D3D11 Resources do we need to do the copying?
CopyingTexture2DWrapper::CopyingTexture2DWrapper(
    const gfx::Size& size,
    std::unique_ptr<Texture2DWrapper> output_wrapper,
    scoped_refptr<VideoProcessorProxy> processor,
    ComD3D11Texture2D output_texture)
    : size_(size),
      video_processor_(std::move(processor)),
      output_texture_wrapper_(std::move(output_wrapper)),
      output_texture_(std::move(output_texture)) {}

CopyingTexture2DWrapper::~CopyingTexture2DWrapper() = default;

// Copy path doesn't need to sync until calling VideoProcessorBlt.
D3D11Status CopyingTexture2DWrapper::BeginSharedImageAccess() {
  return D3D11Status::Codes::kOk;
}

D3D11Status CopyingTexture2DWrapper::ProcessTexture(
    const gfx::ColorSpace& input_color_space,
    scoped_refptr<gpu::ClientSharedImage>& shared_image_dest) {
  // Acquire keyed mutex for VideoProcessorBlt ops.
  D3D11Status status = output_texture_wrapper_->BeginSharedImageAccess();
  if (!status.is_ok()) {
    return status;
  }

  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {
      D3D11_VPOV_DIMENSION_TEXTURE2D};
  output_view_desc.Texture2D.MipSlice = 0;
  ComD3D11VideoProcessorOutputView output_view;
  HRESULT hr = video_processor_->CreateVideoProcessorOutputView(
      output_texture_.Get(), &output_view_desc, &output_view);
  if (!SUCCEEDED(hr))
    return {D3D11Status::Codes::kCreateVideoProcessorOutputViewFailed, hr};

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {0};
  input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  input_view_desc.Texture2D.ArraySlice = array_slice_;
  input_view_desc.Texture2D.MipSlice = 0;
  ComD3D11VideoProcessorInputView input_view;
  hr = video_processor_->CreateVideoProcessorInputView(
      texture_.Get(), &input_view_desc, &input_view);
  if (!SUCCEEDED(hr))
    return {D3D11Status::Codes::kCreateVideoProcessorInputViewFailed};

  D3D11_VIDEO_PROCESSOR_STREAM streams = {0};
  streams.Enable = TRUE;
  streams.pInputSurface = input_view.Get();

  // If the input color space has changed, or if this is the first call, then
  // notify the video processor about it.
  if (!previous_input_color_space_ ||
      *previous_input_color_space_ != input_color_space) {
    previous_input_color_space_ = input_color_space;

    video_processor_->SetStreamColorSpace(input_color_space);
    video_processor_->SetOutputColorSpace(input_color_space);
  }

  hr = video_processor_->VideoProcessorBlt(output_view.Get(),
                                           0,  // output_frameno
                                           1,  // stream_count
                                           &streams);
  if (!SUCCEEDED(hr))
    return {D3D11Status::Codes::kVideoProcessorBltFailed, hr};

  return output_texture_wrapper_->ProcessTexture(input_color_space,
                                                 shared_image_dest);
}

D3D11Status CopyingTexture2DWrapper::Init(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferHelperCB get_helper_cb,
    ComD3D11Texture2D texture,
    size_t array_slice,
    scoped_refptr<media::D3D11PictureBuffer> picture_buffer,
    PictureBufferGPUResourceInitDoneCB
        picture_buffer_gpu_resource_init_done_cb) {
  auto result = video_processor_->Init(size_.width(), size_.height());
  if (!result.is_ok())
    return std::move(result).AddHere();

  // Remember the texture + array_slice so later, ProcessTexture can still use
  // it.
  texture_ = texture;
  array_slice_ = array_slice;

  return output_texture_wrapper_->Init(
      std::move(gpu_task_runner), std::move(get_helper_cb), output_texture_,
      /*array_size=*/0, std::move(picture_buffer),
      std::move(picture_buffer_gpu_resource_init_done_cb));
}

}  // namespace media
