// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_VIDEO_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_VIDEO_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/video_stub.h"

namespace remoting::protocol {

class VideoFeedbackStub;

class HostVideoDispatcher : public ChannelDispatcherBase, public VideoStub {
 public:
  HostVideoDispatcher();

  HostVideoDispatcher(const HostVideoDispatcher&) = delete;
  HostVideoDispatcher& operator=(const HostVideoDispatcher&) = delete;

  ~HostVideoDispatcher() override;

  void set_video_feedback_stub(VideoFeedbackStub* video_feedback_stub) {
    video_feedback_stub_ = video_feedback_stub;
  }

  // VideoStub interface.
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> packet,
                          base::OnceClosure done) override;

 private:
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;

  raw_ptr<VideoFeedbackStub, DanglingUntriaged> video_feedback_stub_ = nullptr;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_HOST_VIDEO_DISPATCHER_H_
