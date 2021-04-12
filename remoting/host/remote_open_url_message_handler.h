// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_MESSAGE_HANDLER_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_MESSAGE_HANDLER_H_

#include "remoting/protocol/named_message_pipe_handler.h"

namespace remoting {

class RemoteOpenUrlMessageHandler final
    : public protocol::NamedMessagePipeHandler {
 public:
  static constexpr char kChannelName[] = "remote-open-url";

  RemoteOpenUrlMessageHandler(const std::string& name,
                              std::unique_ptr<protocol::MessagePipe> pipe);
  ~RemoteOpenUrlMessageHandler() override;

  // protocol::NamedMessagePipeHandler implementation.
  void OnConnected() override;
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;

  RemoteOpenUrlMessageHandler(const RemoteOpenUrlMessageHandler&) = delete;
  RemoteOpenUrlMessageHandler& operator=(const RemoteOpenUrlMessageHandler&) =
      delete;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_MESSAGE_HANDLER_H_
