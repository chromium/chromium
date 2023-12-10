// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/mojom_devtools_agent_host.h"

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/public/browser/mojom_devtools_agent_host_delegate.h"

namespace content {

// static
void MojomDevToolsAgentHost::GetAll(DevToolsAgentHost::List* out_list) {
  for (auto& id : host_ids()) {
    auto host = GetForId(id);
    if (host) {
      out_list->emplace_back(host);
    }
  }
}

MojomDevToolsAgentHost::MojomDevToolsAgentHost(
    const std::string& id,
    std::unique_ptr<MojomDevToolsAgentHostDelegate> delegate)
    : DevToolsAgentHostImpl(id), delegate_(std::move(delegate)) {
  mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgent> agent;
  delegate_->ConnectDevToolsAgent(agent.InitWithNewEndpointAndPassReceiver());
  associated_agent_remote_.Bind(std::move(agent));
  NotifyCreated();
  host_ids().emplace_back(GetId());
}

MojomDevToolsAgentHost::~MojomDevToolsAgentHost() {
  associated_agent_remote_.reset();
  delegate_.reset();
  std::erase(host_ids(), GetId());
}

// Devtools Agent host overrides:

std::string MojomDevToolsAgentHost::GetType() {
  return delegate_->GetType();
}

std::string MojomDevToolsAgentHost::GetTitle() {
  return delegate_->GetTitle();
}

GURL MojomDevToolsAgentHost::GetURL() {
  return delegate_->GetURL();
}

bool MojomDevToolsAgentHost::Activate() {
  return delegate_->Activate();
}

bool MojomDevToolsAgentHost::Close() {
  return delegate_->Close();
}

void MojomDevToolsAgentHost::Reload() {
  delegate_->Reload();
}

// Devtools agent host impl overrides:

bool MojomDevToolsAgentHost::AttachSession(DevToolsSession* session,
                                           bool aquire_wake_lock) {
  session->AttachToAgent(associated_agent_remote_.get(),
                         delegate_->ForceIOSession());
  return true;
}

// static
std::vector<std::string>& MojomDevToolsAgentHost::host_ids() {
  static base::NoDestructor<std::vector<std::string>> host_ids_{};
  return *host_ids_;
}

}  // namespace content
