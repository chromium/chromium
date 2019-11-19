// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "remoting/base/session_options.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "remoting/codec/webrtc_video_encoder_selector.h"
#include "remoting/protocol/host_video_stats_dispatcher.h"
#include "remoting/protocol/video_stream.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/common_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace webrtc {
class PeerConnectionInterface;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class HostVideoStatsDispatcher;
class WebrtcFrameScheduler;
class WebrtcTransport;

class WebrtcVideoStream : public VideoStream,
                          public webrtc::DesktopCapturer::Callback,
                          public HostVideoStatsDispatcher::EventHandler {
 public:
  explicit WebrtcVideoStream(const SessionOptions& options);
  ~WebrtcVideoStream() override;

  void Start(std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
             WebrtcTransport* webrtc_transport,
             scoped_refptr<base::SequencedTaskRunner> encode_task_runner);

  // VideoStream interface.
  void SetEventTimestampsSource(scoped_refptr<InputEventTimestampsSource>
                                    event_timestamps_source) override;
  void Pause(bool pause) override;
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void SetObserver(Observer* observer) override;
  void SelectSource(int id) override;

 private:
  struct FrameStats;

  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // HostVideoStatsDispatcher::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;
  void OnChannelClosed(ChannelDispatcherBase* channel_dispatcher) override;

  // Called by the |scheduler_|.
  void CaptureNextFrame();

  void OnFrameEncoded(WebrtcVideoEncoder::EncodeResult encode_result,
                      std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame);

  void OnEncoderCreated(webrtc::VideoCodecType codec_type);

  // Helper functions to create software encoders that run on the encode thread.
  std::unique_ptr<WebrtcVideoEncoder> CreateVP8Encoder();
  std::unique_ptr<WebrtcVideoEncoder> CreateVP9Encoder();

  // Capturer used to capture the screen.
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
  // Used to send across encoded frames.
  WebrtcTransport* webrtc_transport_ = nullptr;
  // Task runner used by software encoders.
  scoped_refptr<base::SequencedTaskRunner> encode_task_runner_;
  // Used to encode captured frames.
  std::unique_ptr<WebrtcVideoEncoder> encoder_;

  scoped_refptr<InputEventTimestampsSource> event_timestamps_source_;

  scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

  HostVideoStatsDispatcher video_stats_dispatcher_;

  // Stats of the frame that's being captured.
  std::unique_ptr<FrameStats> current_frame_stats_;

  std::unique_ptr<WebrtcFrameScheduler> scheduler_;

  webrtc::DesktopSize frame_size_;
  webrtc::DesktopVector frame_dpi_;
  Observer* observer_ = nullptr;

  WebrtcVideoEncoderSelector encoder_selector_;

  base::ThreadChecker thread_checker_;

  const SessionOptions session_options_;

  // Settings that are received from video-control messages. These are stored
  // here in case a message is received before the encoder is created.
  bool lossless_encode_ = false;
  bool lossless_color_ = false;

  base::WeakPtrFactory<WebrtcVideoStream> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoStream);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
