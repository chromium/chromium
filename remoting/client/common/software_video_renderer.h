// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_COMMON_SOFTWARE_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_COMMON_SOFTWARE_VIDEO_RENDERER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/protocol/video_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

class VideoDecoder;

namespace protocol {
class FrameConsumer;
class FrameStatsConsumer;
}  // namespace protocol

// Implementation of VideoRenderer interface that decodes video frames using the
// CPU (on a dedicated decode thread) and then passes the decoded frames to a
// FrameConsumer.
class SoftwareVideoRenderer : public protocol::VideoRenderer,
                              public protocol::VideoStub {
 public:
  explicit SoftwareVideoRenderer(protocol::FrameConsumer* consumer);

  SoftwareVideoRenderer(const SoftwareVideoRenderer&) = delete;
  SoftwareVideoRenderer& operator=(const SoftwareVideoRenderer&) = delete;

  ~SoftwareVideoRenderer() override;

  // VideoRenderer interface.
  bool Initialize(const ClientContext& client_context,
                  protocol::FrameStatsConsumer* stats_consumer) override;
  void OnSessionConfig(const protocol::SessionConfig& config) override;
  protocol::VideoStub* GetVideoStub() override;
  protocol::FrameConsumer* GetFrameConsumer() override;
  protocol::FrameStatsConsumer* GetFrameStatsConsumer() override;

  // protocol::VideoStub interface.
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> packet,
                          base::OnceClosure done) override;

 private:
  void RenderFrame(base::OnceClosure done,
                   std::unique_ptr<webrtc::DesktopFrame> frame);

  scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner_;

  const raw_ptr<protocol::FrameConsumer> consumer_;

  std::unique_ptr<VideoDecoder> decoder_;

  webrtc::DesktopSize source_size_;
  webrtc::DesktopVector source_dpi_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<SoftwareVideoRenderer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_COMMON_SOFTWARE_VIDEO_RENDERER_H_
