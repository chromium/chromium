// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_processor_proxy.h"

namespace media {

VideoProcessorProxy::~VideoProcessorProxy() {}

VideoProcessorProxy::VideoProcessorProxy(
    ComD3D11VideoDevice video_device,
    ComD3D11DeviceContext d3d11_device_context)
    : video_device_(video_device), device_context_(d3d11_device_context) {}

bool VideoProcessorProxy::Init(uint32_t width, uint32_t height) {
  processor_enumerator_.Reset();
  video_processor_.Reset();

  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = width;
  desc.InputHeight = height;
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = width;
  desc.OutputHeight = height;
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  if (!SUCCEEDED(video_device_->CreateVideoProcessorEnumerator(
          &desc, &processor_enumerator_)))
    return false;

  if (!SUCCEEDED(video_device_->CreateVideoProcessor(
          processor_enumerator_.Get(), 0, &video_processor_)))
    return false;

  if (!SUCCEEDED(device_context_.As(&video_context_)))
    return false;

  return true;
}

HRESULT VideoProcessorProxy::CreateVideoProcessorOutputView(
    ID3D11Texture2D* output_texture,
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC* output_view_descriptor,
    ID3D11VideoProcessorOutputView** output_view) {
  return video_device_->CreateVideoProcessorOutputView(
      output_texture, processor_enumerator_.Get(), output_view_descriptor,
      output_view);
}

HRESULT VideoProcessorProxy::CreateVideoProcessorInputView(
    ID3D11Texture2D* input_texture,
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC* input_view_descriptor,
    ID3D11VideoProcessorInputView** input_view) {
  return video_device_->CreateVideoProcessorInputView(
      input_texture, processor_enumerator_.Get(), input_view_descriptor,
      input_view);
}

HRESULT VideoProcessorProxy::VideoProcessorBlt(
    ID3D11VideoProcessorOutputView* output_view,
    UINT output_frameno,
    UINT stream_count,
    D3D11_VIDEO_PROCESSOR_STREAM* streams) {
  return video_context_->VideoProcessorBlt(video_processor_.Get(), output_view,
                                           output_frameno, stream_count,
                                           streams);
}

}  // namespace media
