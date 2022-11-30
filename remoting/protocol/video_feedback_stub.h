// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_FEEDBACK_STUB_H_
#define REMOTING_PROTOCOL_VIDEO_FEEDBACK_STUB_H_

#include <memory>

namespace remoting {

class VideoAck;

namespace protocol {

class VideoFeedbackStub {
 public:
  VideoFeedbackStub(const VideoFeedbackStub&) = delete;
  VideoFeedbackStub& operator=(const VideoFeedbackStub&) = delete;

  virtual void ProcessVideoAck(std::unique_ptr<VideoAck> video_ack) = 0;

 protected:
  VideoFeedbackStub() = default;
  virtual ~VideoFeedbackStub() = default;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_FEEDBACK_STUB_H_
