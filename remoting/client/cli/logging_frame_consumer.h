// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLI_LOGGING_FRAME_CONSUMER_H_
#define REMOTING_CLIENT_CLI_LOGGING_FRAME_CONSUMER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "remoting/protocol/frame_consumer.h"

namespace webrtc {
class DesktopFrame;
class DesktopSize;
}  // namespace webrtc

namespace remoting {

// Implementation of the protocol::FrameConsumer interface which allocates
// memory and logs frame info to the console.
class LoggingFrameConsumer : public protocol::FrameConsumer {
 public:
  LoggingFrameConsumer();

  LoggingFrameConsumer(const LoggingFrameConsumer&) = delete;
  LoggingFrameConsumer& operator=(const LoggingFrameConsumer&) = delete;

  ~LoggingFrameConsumer() override;

  // protocol::FrameConsumer interface.
  std::unique_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) override;
  void DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                 base::OnceClosure done) override;
  PixelFormat GetPixelFormat() override;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLI_LOGGING_FRAME_CONSUMER_H_
