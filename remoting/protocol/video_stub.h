// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_STUB_H_
#define REMOTING_PROTOCOL_VIDEO_STUB_H_

#include <memory>

#include "base/functional/callback_forward.h"

namespace remoting {

class VideoPacket;

namespace protocol {

class VideoStub {
 public:
  VideoStub(const VideoStub&) = delete;
  VideoStub& operator=(const VideoStub&) = delete;

  virtual void ProcessVideoPacket(std::unique_ptr<VideoPacket> video_packet,
                                  base::OnceClosure done) = 0;

 protected:
  VideoStub() = default;
  virtual ~VideoStub() = default;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_STUB_H_
