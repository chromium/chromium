// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_encoder_factory.h"

#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "remoting/protocol/webrtc_video_encoder_wrapper.h"
#include "third_party/webrtc/api/video_codecs/video_codec.h"

#if defined(USE_H264_ENCODER)
#include "remoting/codec/webrtc_video_encoder_gpu.h"
#endif

namespace remoting::protocol {

WebrtcVideoEncoderFactory::WebrtcVideoEncoderFactory()
    : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
#if defined(USE_H264_ENCODER)
  // This call will query the underlying media classes to determine whether
  // hardware encoding is supported or not. We use a default resolution and
  // framerate so the call doesn't fail due to invalid params.
  if (WebrtcVideoEncoderGpu::IsSupportedByH264({{1920, 1080}, 30})) {
    supported_formats_.emplace_back("H264");
  }
#endif
}

WebrtcVideoEncoderFactory::~WebrtcVideoEncoderFactory() = default;

std::unique_ptr<webrtc::VideoEncoder> WebrtcVideoEncoderFactory::Create(
    const webrtc::Environment& /*env*/,
    const webrtc::SdpVideoFormat& format) {
  return std::make_unique<WebrtcVideoEncoderWrapper>(
      format, session_options_, main_task_runner_,
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::HIGHEST},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED),
      event_router_.GetWeakPtr());
}

std::vector<webrtc::SdpVideoFormat>
WebrtcVideoEncoderFactory::GetSupportedFormats() const {
  return supported_formats_;
}

void WebrtcVideoEncoderFactory::ApplySessionOptions(
    const SessionOptions& options) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  session_options_ = options;
}

}  // namespace remoting::protocol
