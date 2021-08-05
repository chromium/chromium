// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
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
struct FrameStats;
class FrameStatsConsumer;
}  // namespace protocol

// Implementation of VideoRenderer interface that decodes frame on CPU (on a
// decode thread) and then passes decoded frames to a FrameConsumer.
class SoftwareVideoRenderer : public protocol::VideoRenderer,
                              public protocol::VideoStub {
 public:
  // The renderer can be created on any thread but afterwards all methods must
  // be called on the same thread.
  explicit SoftwareVideoRenderer(protocol::FrameConsumer* consumer);

  // Same as above, but take ownership of the |consumer|.
  explicit SoftwareVideoRenderer(
      std::unique_ptr<protocol::FrameConsumer> consumer);

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
  void RenderFrame(std::unique_ptr<protocol::FrameStats> stats,
                   base::OnceClosure done,
                   std::unique_ptr<webrtc::DesktopFrame> frame);
  void OnFrameRendered(std::unique_ptr<protocol::FrameStats> stats,
                       base::OnceClosure done);

  scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner_;

  // |owned_consumer_| and |consumer_| should refer to the same object if
  // |owned_consumer_| is not null.
  std::unique_ptr<protocol::FrameConsumer> owned_consumer_;
  protocol::FrameConsumer* const consumer_;

  protocol::FrameStatsConsumer* stats_consumer_ = nullptr;

  std::unique_ptr<VideoDecoder> decoder_;

  webrtc::DesktopSize source_size_;
  webrtc::DesktopVector source_dpi_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SoftwareVideoRenderer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SoftwareVideoRenderer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_SOFTWARE_VIDEO_RENDERER_H_
