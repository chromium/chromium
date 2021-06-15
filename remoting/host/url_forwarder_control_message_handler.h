// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_URL_FORWARDER_CONTROL_MESSAGE_HANDLER_H_
#define REMOTING_HOST_URL_FORWARDER_CONTROL_MESSAGE_HANDLER_H_

#include "remoting/protocol/named_message_pipe_handler.h"

#include "base/memory/weak_ptr.h"
#include "remoting/proto/url_forwarder_control.pb.h"

namespace remoting {

// Message handler for the url-forwarder-control data channel.
class UrlForwarderControlMessageHandler final
    : public protocol::NamedMessagePipeHandler {
 public:
  static constexpr char kDataChannelName[] = "url-forwarder-control";

  UrlForwarderControlMessageHandler(
      const std::string& name,
      std::unique_ptr<protocol::MessagePipe> pipe);
  ~UrlForwarderControlMessageHandler() override;

  // protocol::NamedMessagePipeHandler implementation.
  void OnConnected() override;
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;

  UrlForwarderControlMessageHandler(const UrlForwarderControlMessageHandler&) =
      delete;
  UrlForwarderControlMessageHandler& operator=(
      const UrlForwarderControlMessageHandler&) = delete;

 private:
  void OnIsUrlForwarderSetUpResult(bool is_set_up);
  void OnSetUpUrlForwarderResult(
      protocol::UrlForwarderControl::SetUpUrlForwarderResponse::State state);

  base::WeakPtrFactory<UrlForwarderControlMessageHandler> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_URL_FORWARDER_CONTROL_MESSAGE_HANDLER_H_
