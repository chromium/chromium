// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_track_source.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/protocol/webrtc_video_frame_adapter.h"

namespace remoting::protocol {

WebrtcVideoTrackSource::WebrtcVideoTrackSource(
    AddSinkCallback add_sink_callback)
    : add_sink_callback_(add_sink_callback),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}
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

std::optional<bool> WebrtcVideoTrackSource::needs_denoising() const {
  return std::nullopt;
}

bool WebrtcVideoTrackSource::GetStats(
    webrtc::VideoTrackSourceInterface::Stats* stats) {
  return false;
}

void WebrtcVideoTrackSource::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  DCHECK(sink);
  if (sink_ && (sink != sink_)) {
    // The same sink can be added more than once, but there should only be 1
    // in total.
    LOG(WARNING) << "More than one sink added, only the latest will be used.";
  }
  sink_ = sink;
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindRepeating(add_sink_callback_, wants));
}

void WebrtcVideoTrackSource::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  DCHECK(sink);
  if (sink != sink_) {
    // This might happen if more than one sink was added.
    LOG(WARNING) << "RemoveSink() called with unexpected sink.";
    return;
  }
  sink_ = nullptr;
}

bool WebrtcVideoTrackSource::SupportsEncodedOutput() const {
  return false;
}

void WebrtcVideoTrackSource::GenerateKeyFrame() {}

void WebrtcVideoTrackSource::AddEncodedSink(
    rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) {}

void WebrtcVideoTrackSource::RemoveEncodedSink(
    rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) {}

void WebrtcVideoTrackSource::SendCapturedFrame(
    std::unique_ptr<webrtc::DesktopFrame> desktop_frame,
    std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats) {
  if (!sink_) {
    LOG(WARNING) << "No sink registered, dropping frame.";
    return;
  }

  webrtc::VideoFrame video_frame = WebrtcVideoFrameAdapter::CreateVideoFrame(
      std::move(desktop_frame), std::move(frame_stats));

  // Wrap-around behavior of '++' is defined for unsigned types.
  // VideoFrame::id() is a 16-bit number which could wrap back to 0 many times
  // during a connection.
  video_frame.set_id(frame_id_++);

  sink_->OnFrame(video_frame);
}

}  // namespace remoting::protocol
