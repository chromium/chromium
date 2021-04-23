// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_TRACK_SOURCE_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_TRACK_SOURCE_H_

#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/notifier.h"

namespace remoting {
namespace protocol {

class WebrtcVideoTrackSource
    : public webrtc::Notifier<webrtc::VideoTrackSourceInterface> {
 public:
  // |add_sink_callback| is notified on the main thread whenever a sink is
  // added or updated.
  explicit WebrtcVideoTrackSource(base::RepeatingClosure add_sink_callback);
  ~WebrtcVideoTrackSource() override;
  WebrtcVideoTrackSource(const WebrtcVideoTrackSource&) = delete;
  WebrtcVideoTrackSource& operator=(const WebrtcVideoTrackSource&) = delete;

  // VideoTrackSourceInterface implementation.
  SourceState state() const override;
  bool remote() const override;
  bool is_screencast() const override;
  absl::optional<bool> needs_denoising() const override;
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

 private:
  base::RepeatingClosure add_sink_callback_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_TRACK_SOURCE_H_
