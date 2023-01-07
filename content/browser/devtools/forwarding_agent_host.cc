// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/forwarding_agent_host.h"

#include "content/browser/devtools/devtools_session.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"

namespace content {

ForwardingAgentHost::ForwardingAgentHost(
    const std::string& id,
    std::unique_ptr<DevToolsExternalAgentProxyDelegate> delegate)
      : DevToolsAgentHostImpl(id),
        delegate_(std::move(delegate)) {
  NotifyCreated();
}

ForwardingAgentHost::~ForwardingAgentHost() = default;

bool ForwardingAgentHost::AttachSession(DevToolsSession* session,
                                        bool acquire_wake_lock) {
  session->TurnIntoExternalProxy(delegate_.get());
  return true;
}

void ForwardingAgentHost::DetachSession(DevToolsSession* session) {}

std::string ForwardingAgentHost::GetType() {
  return delegate_->GetType();
}

std::string ForwardingAgentHost::GetTitle() {
  return delegate_->GetTitle();
}

GURL ForwardingAgentHost::GetURL() {
  return delegate_->GetURL();
}

GURL ForwardingAgentHost::GetFaviconURL() {
  return delegate_->GetFaviconURL();
}

std::string ForwardingAgentHost::GetFrontendURL() {
  return delegate_->GetFrontendURL();
}

bool ForwardingAgentHost::Activate() {
  return delegate_->Activate();
}

void ForwardingAgentHost::Reload() {
  delegate_->Reload();
}

bool ForwardingAgentHost::Close() {
  return delegate_->Close();
}

base::TimeTicks ForwardingAgentHost::GetLastActivityTime() {
  return delegate_->GetLastActivityTime();
}

std::string ForwardingAgentHost::GetDescription() {
  return delegate_->GetDescription();
}

}  // content
