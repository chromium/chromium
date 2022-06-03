// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_CLIENT_CHANNEL_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_CLIENT_CHANNEL_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host_client.h"

namespace content {
// A channel, that is, an open session/connection for sending messages to a
// DevToolsAgentHostClient.
//
// The channel expects CBOR (binary format) encoded inputs.
//
// It transcodes to JSON if the underlying DevToolsAgentHostClient specifies
// DevToolsAgentHostClient::UsesBinaryProtocol() == false.
//
// It inserts the session id, if the underlying session is a child session
// in flatten mode. See also the documentation for the DevTools protocol
// methods Target.attachToTarget and Target.setAutoAttach.
//
// To obtain a client channel, embedders override
// DevToolsManagerDelegate::ClientAttached.
class CONTENT_EXPORT DevToolsAgentHostClientChannel {
 public:
  // |message| must be in binary format (CBOR encoded).
  virtual void DispatchProtocolMessageToClient(
      std::vector<uint8_t> message) = 0;

  // The agent host which will be identified to the client as the sender.
  virtual DevToolsAgentHost* GetAgentHost() = 0;

  // The client to which the messages will be dispatched.
  virtual DevToolsAgentHostClient* GetClient() = 0;

  virtual ~DevToolsAgentHostClientChannel() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_CLIENT_CHANNEL_H_
