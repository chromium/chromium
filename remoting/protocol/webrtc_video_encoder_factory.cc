// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_encoder_factory.h"

#include "base/check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "remoting/protocol/webrtc_video_encoder_wrapper.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"

namespace remoting {
namespace protocol {

WebrtcVideoEncoderFactory::WebrtcVideoEncoderFactory()
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  formats_.push_back(webrtc::SdpVideoFormat("VP8"));
  formats_.push_back(webrtc::SdpVideoFormat("VP9"));
  formats_.push_back(
      webrtc::SdpVideoFormat("VP9", {{webrtc::kVP9FmtpProfileId, "1"}}));
#if defined(USE_H264_ENCODER)
  formats_.push_back(webrtc::SdpVideoFormat("H264"));
#endif
}

WebrtcVideoEncoderFactory::~WebrtcVideoEncoderFactory() = default;

std::unique_ptr<webrtc::VideoEncoder>
WebrtcVideoEncoderFactory::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format) {
  return std::make_unique<WebrtcVideoEncoderWrapper>(
      format, main_task_runner_, video_channel_state_observer_);
}

std::vector<webrtc::SdpVideoFormat>
WebrtcVideoEncoderFactory::GetSupportedFormats() const {
  return formats_;
}

void WebrtcVideoEncoderFactory::SetVideoChannelStateObserver(
    base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  video_channel_state_observer_ = video_channel_state_observer;
}

}  // namespace protocol
}  // namespace remoting
