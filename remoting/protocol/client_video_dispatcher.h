// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_VIDEO_DISPATCHER_H_
#define REMOTING_PROTOCOL_CLIENT_VIDEO_DISPATCHER_H_

#include <list>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {
namespace protocol {

class ClientStub;
class VideoStub;

class ClientVideoDispatcher : public ChannelDispatcherBase {
 public:
  ClientVideoDispatcher(VideoStub* video_stub, ClientStub* client_stub);
  ~ClientVideoDispatcher() override;

 private:
  struct PendingFrame;
  typedef std::list<PendingFrame> PendingFramesList;

  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;

  // Callback for VideoStub::ProcessVideoPacket().
  void OnPacketDone(PendingFramesList::iterator pending_frame);

  PendingFramesList pending_frames_;

  VideoStub* video_stub_;
  ClientStub* client_stub_;

  webrtc::DesktopSize screen_size_;
  webrtc::DesktopVector screen_dpi_ =
      webrtc::DesktopVector(kDefaultDpi, kDefaultDpi);

  base::WeakPtrFactory<ClientVideoDispatcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientVideoDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_VIDEO_DISPATCHER_H_
