// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_VIDEO_STATE_DISPATCHER_H_
#define REMOTING_PROTOCOL_CLIENT_VIDEO_STATE_DISPATCHER_H_

#include <list>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {
namespace protocol {

class VideoStatsStub;

class ClientVideoStatsDispatcher : public ChannelDispatcherBase {
 public:
  ClientVideoStatsDispatcher(const std::string& stream_name,
                             VideoStatsStub* video_stats_stub);
  ~ClientVideoStatsDispatcher() override;

 private:
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;

  VideoStatsStub* video_stats_stub_;

  DISALLOW_COPY_AND_ASSIGN(ClientVideoStatsDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_VIDEO_STATE_DISPATCHER_H_
