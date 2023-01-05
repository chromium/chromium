// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/audits_handler.h"

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_issue_storage.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"

namespace content {
namespace protocol {

AuditsHandler::AuditsHandler()
    : DevToolsDomainHandler(Audits::Metainfo::domainName) {}
AuditsHandler::~AuditsHandler() = default;

// static
std::vector<AuditsHandler*> AuditsHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<AuditsHandler>(Audits::Metainfo::domainName);
}

void AuditsHandler::SetRenderer(int process_host_id,
                                RenderFrameHostImpl* frame_host) {
  host_ = frame_host;
}

void AuditsHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Audits::Frontend>(dispatcher->channel());
  Audits::Dispatcher::wire(dispatcher, this);
}

DispatchResponse AuditsHandler::Disable() {
  enabled_ = false;
  return Response::FallThrough();
}

namespace {

void SendStoredIssuesForFrameToAgent(RenderFrameHostImpl* rfh,
                                     protocol::AuditsHandler* handler) {
  // Check the storage first. No need to do any work in case its empty.
  DevToolsIssueStorage* issue_storage =
      DevToolsIssueStorage::GetForPage(rfh->GetOutermostMainFrame()->GetPage());
  if (!issue_storage)
    return;
  auto issues = issue_storage->FindIssuesForAgentOf(rfh);
  for (auto* const issue : issues) {
    handler->OnIssueAdded(issue);
  }
}

}  // namespace

DispatchResponse AuditsHandler::Enable() {
  if (enabled_) {
    return Response::FallThrough();
  }

  enabled_ = true;
  if (host_) {
    SendStoredIssuesForFrameToAgent(host_, this);
  }

  return Response::FallThrough();
}

void AuditsHandler::OnIssueAdded(
    const protocol::Audits::InspectorIssue* issue) {
  if (enabled_) {
    frontend_->IssueAdded(issue->Clone());
  }
}

}  // namespace protocol
}  // namespace content
