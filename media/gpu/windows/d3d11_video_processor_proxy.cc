// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_processor_proxy.h"

#include "media/base/media_serializers.h"
#include "ui/gfx/color_space_win.h"

namespace media {
namespace {

// Only define this method in debug mode.
#if !defined(NDEBUG)

void AddDebugMessages(D3D11Status* error, ComD3D11Device device) {
  // MSDN says that this needs to be casted twice, then GetMessage should
  // be called with a malloc.
  ComD3D11Debug debug_layer;
  if (!SUCCEEDED(device.As(&debug_layer)))
    return;

  ComD3D11InfoQueue message_layer;
  if (!SUCCEEDED(debug_layer.As(&message_layer)))
    return;

  uint64_t messages_count = message_layer->GetNumStoredMessages();
  if (messages_count == 0)
    return;

  std::vector<std::string> messages(messages_count, "");
  for (uint64_t i = 0; i < messages_count; i++) {
    SIZE_T size;
    message_layer->GetMessage(i, nullptr, &size);
    D3D11_MESSAGE* message = reinterpret_cast<D3D11_MESSAGE*>(malloc(size));
    if (!message)  // probably OOM - so just stop trying to get more.
      return;
    message_layer->GetMessage(i, message, &size);
    messages.emplace_back(message->pDescription);
    free(message);
  }

  error->WithData("debug_info", messages);
}

D3D11Status DebugStatus(D3D11Status&& status, ComD3D11Device device) {
  AddDebugMessages(&status, device);
  return std::move(status);
}

#else

D3D11Status DebugStatus(D3D11Status&& status, ComD3D11Device device) {
  return std::move(status);
}

#endif  // !defined(NDEBUG)

}  // namespace

VideoProcessorProxy::~VideoProcessorProxy() {}

VideoProcessorProxy::VideoProcessorProxy(
    ComD3D11VideoDevice video_device,
    ComD3D11DeviceContext d3d11_device_context)
    : video_device_(video_device), device_context_(d3d11_device_context) {}

D3D11Status VideoProcessorProxy::Init(uint32_t width, uint32_t height) {
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

  // Get the device in case we need it for error generation.
  ComD3D11Device device;
  device_context_->GetDevice(&device);

  HRESULT hr = video_device_->CreateVideoProcessorEnumerator(
      &desc, &processor_enumerator_);
  if (!SUCCEEDED(hr)) {
    return DebugStatus({D3D11Status::Codes::kCreateDecoderOutputViewFailed, hr},
                       device);
  }

  hr = video_device_->CreateVideoProcessor(processor_enumerator_.Get(), 0,
                                           &video_processor_);
  if (!SUCCEEDED(hr)) {
    return DebugStatus({D3D11Status::Codes::kCreateVideoProcessorFailed, hr},
                       device);
  }

  hr = device_context_.As(&video_context_);
  if (!SUCCEEDED(hr)) {
    return DebugStatus({D3D11Status::Codes::kQueryVideoContextFailed, hr},
                       device);
  }

  // Turn off auto stream processing (the default) that will hurt power
  // consumption.
  video_context_->VideoProcessorSetStreamAutoProcessingMode(
      video_processor_.Get(), 0, FALSE);

  return D3D11Status::Codes::kOk;
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

void VideoProcessorProxy::SetStreamColorSpace(
    const gfx::ColorSpace& color_space) {
  ComD3D11VideoContext1 video_context1;

  // Try to use the 11.1 interface if possible, else use 11.0.
  if (FAILED(video_context_.As(&video_context1))) {
    // Note that if we have an HDR context but no 11.1 device, then this will
    // likely not work.
    auto d3d11_color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(color_space);
    video_context_->VideoProcessorSetStreamColorSpace(video_processor_.Get(), 0,
                                                      &d3d11_color_space);
  } else {
    video_context1->VideoProcessorSetStreamColorSpace1(
        video_processor_.Get(), 0,
        gfx::ColorSpaceWin::GetDXGIColorSpace(color_space));
  }
}

void VideoProcessorProxy::SetOutputColorSpace(
    const gfx::ColorSpace& color_space) {
  ComD3D11VideoContext1 video_context1;
  if (FAILED(video_context_.As(&video_context1))) {
    // Hopefully, |color_space| is supported, but that's not our problem.
    auto d3d11_color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(color_space);
    video_context_->VideoProcessorSetOutputColorSpace(video_processor_.Get(),
                                                      &d3d11_color_space);
  } else {
    video_context1->VideoProcessorSetOutputColorSpace1(
        video_processor_.Get(),
        gfx::ColorSpaceWin::GetDXGIColorSpace(color_space));
  }
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
