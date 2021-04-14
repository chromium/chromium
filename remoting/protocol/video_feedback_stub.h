// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_FEEDBACK_STUB_H_
#define REMOTING_PROTOCOL_VIDEO_FEEDBACK_STUB_H_

#include <memory>

#include "base/macros.h"

namespace remoting {

class VideoAck;

namespace protocol {

class VideoFeedbackStub {
 public:
  virtual void ProcessVideoAck(std::unique_ptr<VideoAck> video_ack) = 0;

 protected:
  VideoFeedbackStub() {}
  virtual ~VideoFeedbackStub() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoFeedbackStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_FEEDBACK_STUB_H_
