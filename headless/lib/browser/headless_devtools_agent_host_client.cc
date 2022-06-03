// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools_agent_host_client.h"

#include "content/public/browser/devtools_agent_host.h"

namespace headless {

HeadlessDevToolsAgentHostClient::HeadlessDevToolsAgentHostClient(
    scoped_refptr<content::DevToolsAgentHost> agent_host)
    : agent_host_(std::move(agent_host)) {
  agent_host_->AttachClient(this);
}

HeadlessDevToolsAgentHostClient::~HeadlessDevToolsAgentHostClient() {
  if (agent_host_)
    agent_host_->DetachClient(this);
}

void HeadlessDevToolsAgentHostClient::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    base::span<const uint8_t> json_message) {
  DCHECK_EQ(agent_host, agent_host_.get());
  if (client_)
    client_->ReceiveProtocolMessage(json_message);
}

void HeadlessDevToolsAgentHostClient::AgentHostClosed(
    content::DevToolsAgentHost* agent_host) {
  DCHECK_EQ(agent_host, agent_host_.get());
  agent_host_ = nullptr;
  if (client_)
    client_->ChannelClosed();
}

void HeadlessDevToolsAgentHostClient::SetClient(
    HeadlessDevToolsChannel::Client* client) {
  client_ = client;
}

void HeadlessDevToolsAgentHostClient::SendProtocolMessage(
    base::span<const uint8_t> message) {
  if (agent_host_)
    agent_host_->DispatchProtocolMessage(this, message);
}

}  // namespace headless
