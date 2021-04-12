// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url_message_handler.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/proto/remote_open_url.pb.h"

namespace remoting {

// static
constexpr char RemoteOpenUrlMessageHandler::kChannelName[];

RemoteOpenUrlMessageHandler::RemoteOpenUrlMessageHandler(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)) {}

RemoteOpenUrlMessageHandler::~RemoteOpenUrlMessageHandler() = default;

void RemoteOpenUrlMessageHandler::OnConnected() {
  NOTIMPLEMENTED();
  // To send a request: Send(remote_open_url, base::DoNothing())
}

void RemoteOpenUrlMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  NOTIMPLEMENTED();
}

void RemoteOpenUrlMessageHandler::OnDisconnecting() {
  NOTIMPLEMENTED();
}

}  // namespace remoting