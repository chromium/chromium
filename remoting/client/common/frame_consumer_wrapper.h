// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_COMMON_FRAME_CONSUMER_WRAPPER_H_
#define REMOTING_CLIENT_COMMON_FRAME_CONSUMER_WRAPPER_H_

#include "base/memory/raw_ptr.h"
#include "remoting/protocol/video_renderer.h"

namespace remoting {

namespace protocol {
class FrameConsumer;
class FrameStatsConsumer;
class VideoStub;
}  // namespace protocol

// Implementation of VideoRenderer interface that exposes an existing
// FrameConsumer but does no work itself. This wrapper is needed when using
// WebRTC since it handles decoding internally and provides the decoded frame
// directly to the consumer.
class FrameConsumerWrapper : public protocol::VideoRenderer {
 public:
  explicit FrameConsumerWrapper(protocol::FrameConsumer* consumer);

  FrameConsumerWrapper(const FrameConsumerWrapper&) = delete;
  FrameConsumerWrapper& operator=(const FrameConsumerWrapper&) = delete;

  ~FrameConsumerWrapper() override;

  // VideoRenderer interface.
  bool Initialize(const ClientContext& client_context,
                  protocol::FrameStatsConsumer* stats_consumer) override;
  void OnSessionConfig(const protocol::SessionConfig& config) override;
  protocol::VideoStub* GetVideoStub() override;
  protocol::FrameConsumer* GetFrameConsumer() override;
  protocol::FrameStatsConsumer* GetFrameStatsConsumer() override;

 private:
  const raw_ptr<protocol::FrameConsumer> consumer_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_COMMON_FRAME_CONSUMER_WRAPPER_H_
