// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_TEST_VIDEO_RENDERER_H_
#define REMOTING_TEST_TEST_VIDEO_RENDERER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/protocol/video_stub.h"

namespace base {
class Thread;
class SingleThreadTaskRunner;
}

namespace webrtc {
class DesktopFrame;
class DesktopRect;
}

namespace remoting {
namespace test {

struct RGBValue;

// Processes video packets as they are received from the remote host. Must be
// used from a thread running a message loop and this class will use that
// message loop to execute the done callbacks passed by the caller of
// ProcessVideoPacket.
class TestVideoRenderer : public protocol::VideoRenderer,
                          public protocol::VideoStub {
 public:
  TestVideoRenderer();
  ~TestVideoRenderer() override;

  // VideoRenderer interface.
  bool Initialize(const ClientContext& client_context,
                  protocol::FrameStatsConsumer* stats_consumer) override;
  void OnSessionConfig(const protocol::SessionConfig& config) override;
  protocol::VideoStub* GetVideoStub() override;
  protocol::FrameConsumer* GetFrameConsumer() override;
  protocol::FrameStatsConsumer* GetFrameStatsConsumer() override;

  // protocol::VideoStub interface.
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> video_packet,
                          base::OnceClosure done) override;

  // Initialize a decoder to decode video packets.
  void SetCodecForDecoding(const protocol::ChannelConfig::Codec codec);

  // Returns a copy of the current frame.
  std::unique_ptr<webrtc::DesktopFrame> GetCurrentFrameForTest() const;

  // Gets a weak pointer for this object.
  base::WeakPtr<TestVideoRenderer> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Set expected rect and average color for comparison and the callback will be
  // called when the pattern is matched.
  void ExpectAverageColorInRect(
      const webrtc::DesktopRect& expected_rect,
      const RGBValue& expected_average_color,
      const base::Closure& image_pattern_matched_callback);

  // Turn on/off saving video frames to disk.
  void SaveFrameDataToDisk(bool save_frame_data_to_disk);

 private:
  // The actual implementation resides in Core class.
  class Core;
  std::unique_ptr<Core> core_;

  // Used to ensure TestVideoRenderer methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Used to decode and process video packets.
  std::unique_ptr<base::Thread> video_decode_thread_;

  // Used to post tasks to video decode thread.
  scoped_refptr<base::SingleThreadTaskRunner> video_decode_task_runner_;

  // Used to weakly bind |this| to methods.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<TestVideoRenderer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestVideoRenderer);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_TEST_VIDEO_RENDERER_H_
