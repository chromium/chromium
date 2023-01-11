// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_VIDEO_RENDERER_H_
#define REMOTING_PROTOCOL_FAKE_VIDEO_RENDERER_H_

#include <list>
#include <memory>

#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/protocol/video_stub.h"

namespace remoting::protocol {

class FakeVideoStub : public VideoStub {
 public:
  FakeVideoStub();
  ~FakeVideoStub() override;

  const std::list<std::unique_ptr<VideoPacket>>& received_packets() {
    return received_packets_;
  }

  void set_on_frame_callback(const base::RepeatingClosure& on_frame_callback);

  // VideoStub interface.
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> video_packet,
                          base::OnceClosure done) override;

 private:
  std::list<std::unique_ptr<VideoPacket>> received_packets_;
  base::RepeatingClosure on_frame_callback_;

  THREAD_CHECKER(thread_checker_);
};

class FakeFrameConsumer : public FrameConsumer {
 public:
  FakeFrameConsumer();
  ~FakeFrameConsumer() override;

  const std::list<std::unique_ptr<webrtc::DesktopFrame>>& received_frames() {
    return received_frames_;
  }

  void set_on_frame_callback(const base::RepeatingClosure& on_frame_callback);

  // FrameConsumer interface.
  std::unique_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) override;
  void DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                 base::OnceClosure done) override;
  PixelFormat GetPixelFormat() override;

 private:
  std::list<std::unique_ptr<webrtc::DesktopFrame>> received_frames_;
  base::RepeatingClosure on_frame_callback_;

  THREAD_CHECKER(thread_checker_);
};

class FakeFrameStatsConsumer : public FrameStatsConsumer {
 public:
  FakeFrameStatsConsumer();
  ~FakeFrameStatsConsumer() override;

  const std::list<FrameStats>& received_stats() { return received_stats_; }

  void set_on_stats_callback(const base::RepeatingClosure& on_stats_callback);

  // FrameStatsConsumer interface.
  void OnVideoFrameStats(const FrameStats& stats) override;

 private:
  std::list<FrameStats> received_stats_;
  base::RepeatingClosure on_stats_callback_;

  THREAD_CHECKER(thread_checker_);
};

class FakeVideoRenderer : public VideoRenderer {
 public:
  FakeVideoRenderer();
  ~FakeVideoRenderer() override;

  // VideoRenderer interface.
  bool Initialize(const ClientContext& client_context,
                  protocol::FrameStatsConsumer* stats_consumer) override;
  void OnSessionConfig(const SessionConfig& config) override;
  FakeVideoStub* GetVideoStub() override;
  FakeFrameConsumer* GetFrameConsumer() override;
  FakeFrameStatsConsumer* GetFrameStatsConsumer() override;

 private:
  FakeVideoStub video_stub_;
  FakeFrameConsumer frame_consumer_;
  FakeFrameStatsConsumer frame_stats_consumer_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FAKE_VIDEO_RENDERER_H_
