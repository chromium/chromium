// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FRAME_CONSUMER_H_
#define REMOTING_PROTOCOL_FRAME_CONSUMER_H_

#include <memory>

#include "base/callback_forward.h"

namespace webrtc {
class DesktopFrame;
class DesktopSize;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class FrameConsumer {
 public:
  FrameConsumer(const FrameConsumer&) = delete;
  FrameConsumer& operator=(const FrameConsumer&) = delete;

  virtual ~FrameConsumer() {}

  // List of supported pixel formats needed by various platforms.
  enum PixelFormat {
    FORMAT_BGRA,  // Used by the Pepper plugin.
    FORMAT_RGBA,  // Used for Android's Bitmap class.
  };

  virtual std::unique_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) = 0;

  virtual void DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                         base::OnceClosure done) = 0;

  // Returns the preferred pixel encoding for the platform.
  virtual PixelFormat GetPixelFormat() = 0;

 protected:
  FrameConsumer() {}
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FRAME_CONSUMER_H_
