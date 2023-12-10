// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_TRACK_SOURCE_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_TRACK_SOURCE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/notifier.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

#include <memory>

namespace remoting::protocol {

class WebrtcVideoTrackSource
    : public webrtc::Notifier<webrtc::VideoTrackSourceInterface> {
 public:
  using AddSinkCallback =
      base::RepeatingCallback<void(const rtc::VideoSinkWants& wants)>;

  // |add_sink_callback| is notified on the main thread whenever a sink is
  // added or updated.
  explicit WebrtcVideoTrackSource(AddSinkCallback add_sink_callback);

  ~WebrtcVideoTrackSource() override;
  WebrtcVideoTrackSource(const WebrtcVideoTrackSource&) = delete;

  WebrtcVideoTrackSource& operator=(const WebrtcVideoTrackSource&) = delete;

  // VideoTrackSourceInterface implementation.
  SourceState state() const override;
  bool remote() const override;
  bool is_screencast() const override;
  std::optional<bool> needs_denoising() const override;
  bool GetStats(Stats* stats) override;
  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;
  bool SupportsEncodedOutput() const override;
  void GenerateKeyFrame() override;
  void AddEncodedSink(
      rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override;
  void RemoveEncodedSink(
      rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override;

  // Sends a captured frame to the sink if one was added. The |frame_stats|
  // will be associated with the frame and will be attached to the output
  // EncodedFrame.
  void SendCapturedFrame(
      std::unique_ptr<webrtc::DesktopFrame> desktop_frame,
      std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats);

 private:
  raw_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> sink_ = nullptr;
  AddSinkCallback add_sink_callback_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Incrementing ID to be attached to each VideoFrame, so that the
  // encoder-wrapper can detect if a frame was dropped.
  uint16_t frame_id_ = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_TRACK_SOURCE_H_
