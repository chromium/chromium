// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_track_source.h"

namespace remoting {
namespace protocol {

WebrtcVideoTrackSource::WebrtcVideoTrackSource() = default;
WebrtcVideoTrackSource::~WebrtcVideoTrackSource() = default;

webrtc::MediaSourceInterface::SourceState WebrtcVideoTrackSource::state()
    const {
  return kLive;
}

bool WebrtcVideoTrackSource::remote() const {
  return false;
}

bool WebrtcVideoTrackSource::is_screencast() const {
  return true;
}

absl::optional<bool> WebrtcVideoTrackSource::needs_denoising() const {
  return absl::nullopt;
}

bool WebrtcVideoTrackSource::GetStats(
    webrtc::VideoTrackSourceInterface::Stats* stats) {
  return false;
}

void WebrtcVideoTrackSource::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {}

void WebrtcVideoTrackSource::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {}

bool WebrtcVideoTrackSource::SupportsEncodedOutput() const {
  return false;
}

void WebrtcVideoTrackSource::GenerateKeyFrame() {}

void WebrtcVideoTrackSource::AddEncodedSink(
    rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) {}

void WebrtcVideoTrackSource::RemoveEncodedSink(
    rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) {}

}  // namespace protocol
}  // namespace remoting
