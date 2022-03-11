// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/base/session_options.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "remoting/protocol/video_stream.h"
#include "remoting/protocol/webrtc_video_track_source.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace webrtc {
class PeerConnectionInterface;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class HostVideoStatsDispatcher;
class WebrtcVideoEncoderFactory;
class WebrtcFrameScheduler;
class WebrtcTransport;

class WebrtcVideoStream : public VideoStream,
                          public webrtc::DesktopCapturer::Callback,
                          public VideoChannelStateObserver {
 public:
  WebrtcVideoStream(const std::string& stream_name,
                    const SessionOptions& options);

  WebrtcVideoStream(const WebrtcVideoStream&) = delete;
  WebrtcVideoStream& operator=(const WebrtcVideoStream&) = delete;

  ~WebrtcVideoStream() override;

  void set_video_stats_dispatcher(
      base::WeakPtr<HostVideoStatsDispatcher> video_stats_dispatcher) {
    video_stats_dispatcher_ = video_stats_dispatcher;
  }

  void Start(std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
             WebrtcTransport* webrtc_transport,
             WebrtcVideoEncoderFactory* video_encoder_factory);

  // VideoStream interface.
  void SetEventTimestampsSource(scoped_refptr<InputEventTimestampsSource>
                                    event_timestamps_source) override;
  void Pause(bool pause) override;
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void SetObserver(Observer* observer) override;
  void SelectSource(webrtc::ScreenId id) override;

  // VideoChannelStateObserver interface.
  void OnKeyFrameRequested() override;
  void OnTargetBitrateChanged(int bitrate_kbps) override;
  void OnFrameEncoded(WebrtcVideoEncoder::EncodeResult encode_result,
                      const WebrtcVideoEncoder::EncodedFrame* frame) override;
  void OnEncodedFrameSent(
      webrtc::EncodedImageCallback::Result result,
      const WebrtcVideoEncoder::EncodedFrame& frame) override;

 private:
  struct FrameStats;

  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Called by the |scheduler_|.
  void CaptureNextFrame();

  // Called by |video_track_source_|.
  void OnSinkAddedOrUpdated(const rtc::VideoSinkWants& wants);

  // Label of the associated WebRTC video-stream.
  std::string stream_name_;

  // Capturer used to capture the screen.
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;

  // Used to send captured frames to the encoder.
  rtc::scoped_refptr<WebrtcVideoTrackSource> video_track_source_;

  // The transceiver created for this video-stream.
  rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver_;

  scoped_refptr<InputEventTimestampsSource> event_timestamps_source_;

  scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

  base::WeakPtr<HostVideoStatsDispatcher> video_stats_dispatcher_;

  // Stats of the frame that's being captured.
  std::unique_ptr<FrameStats> current_frame_stats_;

  std::unique_ptr<WebrtcFrameScheduler> scheduler_;

  webrtc::DesktopSize frame_size_;
  webrtc::DesktopVector frame_dpi_;
  raw_ptr<Observer> observer_ = nullptr;

  base::ThreadChecker thread_checker_;

  const SessionOptions session_options_;

  base::WeakPtrFactory<WebrtcVideoStream> weak_factory_{this};
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
