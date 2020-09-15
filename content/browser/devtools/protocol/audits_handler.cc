// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/audits_handler.h"

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_issue_storage.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
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
      DevToolsIssueStorage::GetForCurrentDocument(rfh->GetMainFrame());
  if (!issue_storage)
    return;

  FrameTreeNode* local_root = rfh->frame_tree_node();

  std::vector<int> frame_tree_node_ids;
  for (FrameTreeNode* node : rfh->frame_tree()->SubtreeNodes(local_root)) {
    // For each child we find the child's local root. Should the child's local
    // root match |local_root|, the provided |AuditsHandler| is responsible and
    // we collect the devtools_frame_token.
    if (local_root == GetFrameTreeNodeAncestor(node)) {
      frame_tree_node_ids.push_back(node->frame_tree_node_id());
    }
  }

  base::flat_set<int> frame_ids_set(frame_tree_node_ids);
  auto issues = issue_storage->FilterIssuesBy(std::move(frame_ids_set));
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
    frontend_->IssueAdded(issue->clone());
  }
}

}  // namespace protocol
}  // namespace content
