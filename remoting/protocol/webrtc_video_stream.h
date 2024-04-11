// Copyright 2015 The Chromium Authors
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
#include "remoting/base/constants.h"
#include "remoting/base/session_options.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "remoting/protocol/video_stream.h"
#include "remoting/protocol/webrtc_video_track_source.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class PeerConnectionInterface;
}  // namespace webrtc

namespace remoting::protocol {

class HostVideoStatsDispatcher;
class WebrtcVideoEncoderFactory;
class WebrtcTransport;

class WebrtcVideoStream : public VideoStream, public VideoChannelStateObserver {
 public:
  explicit WebrtcVideoStream(const SessionOptions& options);

  WebrtcVideoStream(const WebrtcVideoStream&) = delete;
  WebrtcVideoStream& operator=(const WebrtcVideoStream&) = delete;

  ~WebrtcVideoStream() override;

  void set_video_stats_dispatcher(
      base::WeakPtr<HostVideoStatsDispatcher> video_stats_dispatcher) {
    video_stats_dispatcher_ = video_stats_dispatcher;
  }

  // |screen_id| should be kFullDesktopScreenId for single-stream mode, or
  // the screen being captured for multi-stream mode.
  void Start(webrtc::ScreenId screen_id,
             std::unique_ptr<DesktopCapturer> desktop_capturer,
             WebrtcTransport* webrtc_transport,
             WebrtcVideoEncoderFactory* video_encoder_factory);

  void SetEventTimestampsSource(scoped_refptr<InputEventTimestampsSource>
                                    event_timestamps_source) override;
  void Pause(bool pause) override;
  void SetObserver(Observer* observer) override;
  void SelectSource(webrtc::ScreenId id) override;
  void SetComposeEnabled(bool enabled) override;
  void SetMouseCursor(
      std::unique_ptr<webrtc::MouseCursor> mouse_cursor) override;
  void SetMouseCursorPosition(const webrtc::DesktopVector& position) override;
  void SetTargetFramerate(int framerate) override;
  void BoostFramerate(base::TimeDelta capture_interval,
                      base::TimeDelta boost_duration) override;

  // Returns the stream name corresponding to the initial `screen_id` passed to
  // Start(). Used for sending VideoLayout messages to the client, which
  // include the stream-name for each display.
  static std::string StreamNameForId(webrtc::ScreenId id);

  // VideoChannelStateObserver interface.
  void OnEncodedFrameSent(
      webrtc::EncodedImageCallback::Result result,
      const WebrtcVideoEncoder::EncodedFrame& frame) override;

 private:
  class Core;
  struct FrameStats;

  // Called by |video_track_source_|.
  void OnSinkAddedOrUpdated(const rtc::VideoSinkWants& wants);

  // Called from |core_|.
  void OnVideoSizeChanged(webrtc::DesktopSize frame_size,
                          webrtc::DesktopVector frame_dpi);
  void SendCapturedFrame(
      std::unique_ptr<webrtc::DesktopFrame> desktop_frame,
      std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats);

  // Store the target framerate so we can set it on the RTP Sender when the SDP
  // is renegotiated (such as when the codec or codec profile is changed).
  int target_framerate_ = kTargetFrameRate;

  // Used to send captured frames to the encoder.
  rtc::scoped_refptr<WebrtcVideoTrackSource> video_track_source_;

  // The transceiver created for this video-stream.
  rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver_;

  scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

  base::WeakPtr<HostVideoStatsDispatcher> video_stats_dispatcher_;

  raw_ptr<Observer> observer_ = nullptr;

  const SessionOptions session_options_;

  std::unique_ptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> core_task_runner_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<WebrtcVideoStream> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
