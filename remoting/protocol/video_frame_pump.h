// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_FRAME_PUMP_H_
#define REMOTING_PROTOCOL_VIDEO_FRAME_PUMP_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/capture_scheduler.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/video_stream.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting::protocol {

class VideoFeedbackStub;
class VideoStub;

// Class responsible for scheduling frame captures from a screen capturer,
// delivering them to a VideoEncoder to encode, and finally passing the encoded
// video packets to the specified VideoStub to send on the network.
//
// THREADING
//
// This class is supplied TaskRunners to use for capture, encode and network
// operations.  Capture, encode and network transmission tasks are interleaved
// as illustrated below:
//
// |       CAPTURE       ENCODE     NETWORK
// |    .............
// |    .  Capture  .
// |    .............
// |                  ............
// |                  .          .
// |    ............. .          .
// |    .  Capture  . .  Encode  .
// |    ............. .          .
// |                  .          .
// |                  ............
// |    ............. ............ ..........
// |    .  Capture  . .          . .  Send  .
// |    ............. .          . ..........
// |                  .  Encode  .
// |                  .          .
// |                  .          .
// |                  ............
// | Time
// v
//
// VideoFramePump would ideally schedule captures so as to saturate the slowest
// of the capture, encode and network processes.  However, it also needs to
// rate-limit captures to avoid overloading the host system, either by consuming
// too much CPU, or hogging the host's graphics subsystem.
class VideoFramePump : public VideoStream,
                       public webrtc::DesktopCapturer::Callback {
 public:
  // Creates a VideoFramePump running capture, encode and network tasks on the
  // supplied TaskRunners. Video will be pumped to |video_stub|, which must
  // outlive the pump..
  VideoFramePump(scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
                 std::unique_ptr<DesktopCapturer> capturer,
                 std::unique_ptr<VideoEncoder> encoder,
                 protocol::VideoStub* video_stub);

  VideoFramePump(const VideoFramePump&) = delete;
  VideoFramePump& operator=(const VideoFramePump&) = delete;

  ~VideoFramePump() override;

  // VideoStream interface.
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

  protocol::VideoFeedbackStub* video_feedback_stub() {
    return &capture_scheduler_;
  }

 private:
  struct FrameTimestamps {
    FrameTimestamps();
    ~FrameTimestamps();

    // The following field is not-null for a single frame after each incoming
    // input event.
    InputEventTimestamps input_event_timestamps;

    base::TimeTicks capture_started_time;
    base::TimeTicks capture_ended_time;
    base::TimeTicks encode_started_time;
    base::TimeTicks encode_ended_time;
    base::TimeTicks can_send_time;
  };

  struct PacketWithTimestamps {
    PacketWithTimestamps(std::unique_ptr<VideoPacket> packet,
                         std::unique_ptr<FrameTimestamps> timestamps);
    ~PacketWithTimestamps();

    std::unique_ptr<VideoPacket> packet;
    std::unique_ptr<FrameTimestamps> timestamps;
  };

  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Callback for CaptureScheduler.
  void CaptureNextFrame();

  // Task running on the encoder thread to encode the |frame|.
  static std::unique_ptr<PacketWithTimestamps> EncodeFrame(
      VideoEncoder* encoder,
      std::unique_ptr<webrtc::DesktopFrame> frame,
      std::unique_ptr<FrameTimestamps> timestamps);

  // Task called when a frame has finished encoding.
  void OnFrameEncoded(std::unique_ptr<PacketWithTimestamps> packet);

  // Sends |packet| to the client.
  void SendPacket(std::unique_ptr<PacketWithTimestamps> packet);

  // Helper called from SendPacket() to calculate timing fields in the |packet|
  // before sending it.
  void UpdateFrameTimers(VideoPacket* packet, FrameTimestamps* timestamps);

  // Callback passed to |video_stub_|.
  void OnVideoPacketSent();

  // Called by |keep_alive_timer_|.
  void SendKeepAlivePacket();

  // Callback for |video_stub_| called after a keep-alive packet is sent.
  void OnKeepAlivePacketSent();

  // Task runner used to run |encoder_|.
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;

  // Capturer used to capture the screen.
  std::unique_ptr<DesktopCapturer> capturer_;

  // Used to encode captured frames. Always accessed on the encode thread.
  std::unique_ptr<VideoEncoder> encoder_;

  scoped_refptr<InputEventTimestampsSource> event_timestamps_source_;

  // Interface through which video frames are passed to the client.
  raw_ptr<protocol::VideoStub> video_stub_;

  raw_ptr<Observer> observer_ = nullptr;
  webrtc::DesktopSize frame_size_;
  webrtc::DesktopVector frame_dpi_;

  // Timer used to ensure that we send empty keep-alive frames to the client
  // even when the video stream is paused or encoder is busy.
  base::RetainingOneShotTimer keep_alive_timer_;

  // CaptureScheduler calls CaptureNextFrame() whenever a new frame needs to be
  // captured.
  CaptureScheduler capture_scheduler_;

  // Timestamps for the frame that's being captured.
  std::unique_ptr<FrameTimestamps> captured_frame_timestamps_;

  bool send_pending_ = false;

  std::vector<std::unique_ptr<PacketWithTimestamps>> pending_packets_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<VideoFramePump> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_VIDEO_FRAME_PUMP_H_
