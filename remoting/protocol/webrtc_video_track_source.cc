// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_track_source.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/protocol/webrtc_video_frame_adapter.h"
#include "third_party/webrtc/api/video/video_frame.h"

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
    webrtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const webrtc::VideoSinkWants& wants) {
  DCHECK(sink);
  if (sink_ && (sink != sink_)) {
    // The same sink can be added more than once, but there should only be 1
    // in total.
    LOG(WARNING) << "More than one sink added, only the latest will be used.";
  }
  sink_ = sink;
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(add_sink_callback_, wants)
          .Then(base::BindOnce(&WebrtcVideoTrackSource::SendPendingFrame,
                               weak_ptr_factory_.GetWeakPtr())));
}

void WebrtcVideoTrackSource::RemoveSink(
    webrtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
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
    webrtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) {}

void WebrtcVideoTrackSource::RemoveEncodedSink(
    webrtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) {}

void WebrtcVideoTrackSource::SendCapturedFrame(
    std::unique_ptr<webrtc::DesktopFrame> desktop_frame,
    std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  webrtc::VideoFrame video_frame = WebrtcVideoFrameAdapter::CreateVideoFrame(
      std::move(desktop_frame), std::move(frame_stats));

  // Wrap-around behavior of '++' is defined for unsigned types.
  // VideoFrame::id() is a 16-bit number which could wrap back to 0 many times
  // during a connection.
  video_frame.set_id(frame_id_++);

  if (sink_) {
    sink_->OnFrame(video_frame);
    return;
  }

  HOST_LOG << "Frame will be sent to the sink after the sink is registered.";
  pending_frame_ = std::make_unique<webrtc::VideoFrame>(std::move(video_frame));
  // Make the entire frame dirty.
  pending_frame_->set_update_rect(webrtc::VideoFrame::UpdateRect{
      .offset_x = 0,
      .offset_y = 0,
      .width = video_frame.width(),
      .height = video_frame.height(),
  });
}

void WebrtcVideoTrackSource::SendPendingFrame() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  if (!pending_frame_) {
    return;
  }
  HOST_LOG << "Sending pending frame to the sink.";
  sink_->OnFrame(std::move(*pending_frame_));
  pending_frame_.reset();
}

}  // namespace remoting::protocol
