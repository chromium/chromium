// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/test/mock_devtools_agent_host.h"

#include "base/memory/ref_counted_memory.h"

namespace content {

std::string MockDevToolsAgentHost::CreateIOStreamFromData(
    scoped_refptr<base::RefCountedMemory>) {
  return std::string();
}

bool MockDevToolsAgentHost::AttachClient(
    content::DevToolsAgentHostClient* client) {
  DCHECK(!client_);
  client_ = client;
  return true;
}

bool MockDevToolsAgentHost::AttachClientWithoutWakeLock(
    content::DevToolsAgentHostClient* client) {
  return AttachClient(client);
}

bool MockDevToolsAgentHost::DetachClient(
    content::DevToolsAgentHostClient* client) {
  if (client != client_) {
    return false;
  }
  client_ = nullptr;
  return true;
}

bool MockDevToolsAgentHost::IsAttached() {
  return client_ != nullptr;
}

void MockDevToolsAgentHost::DispatchProtocolMessage(
    content::DevToolsAgentHostClient* client,
    base::span<const uint8_t> message) {
  DCHECK_EQ(client, client_);
  client->DispatchProtocolMessage(this, message);
}

std::string MockDevToolsAgentHost::GetId() {
  return std::string();
}

std::string MockDevToolsAgentHost::GetParentId() {
  return std::string();
}

std::string MockDevToolsAgentHost::GetOpenerId() {
  return std::string();
}

bool MockDevToolsAgentHost::CanAccessOpener() {
  return true;
}

std::string MockDevToolsAgentHost::GetOpenerFrameId() {
  return std::string();
}

content::WebContents* MockDevToolsAgentHost::GetWebContents() {
  return nullptr;
}

content::BrowserContext* MockDevToolsAgentHost::GetBrowserContext() {
  return nullptr;
}

std::string MockDevToolsAgentHost::GetType() {
  return std::string();
}

std::string MockDevToolsAgentHost::GetTitle() {
  return std::string();
}

std::string MockDevToolsAgentHost::GetDescription() {
  return std::string();
}

GURL MockDevToolsAgentHost::GetURL() {
  return GURL();
}

GURL MockDevToolsAgentHost::GetFaviconURL() {
  return GURL();
}

std::string MockDevToolsAgentHost::GetFrontendURL() {
  return std::string();
}

bool MockDevToolsAgentHost::Activate() {
  return true;
}

bool MockDevToolsAgentHost::Close() {
  return true;
}

base::TimeTicks MockDevToolsAgentHost::GetLastActivityTime() {
  return base::TimeTicks();
}

content::RenderProcessHost* MockDevToolsAgentHost::GetProcessHost() {
  return nullptr;
}

}  // namespace content
