// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_issue_storage.h"

#include "content/browser/devtools/protocol/audits.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace content {

static const unsigned kMaxIssueCount = 1000;

RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(DevToolsIssueStorage)

DevToolsIssueStorage::DevToolsIssueStorage(RenderFrameHost* rfh)
    : WebContentsObserver(content::WebContents::FromRenderFrameHost(rfh)) {}
DevToolsIssueStorage::~DevToolsIssueStorage() = default;

void DevToolsIssueStorage::AddInspectorIssue(
    int frame_tree_node_id,
    std::unique_ptr<protocol::Audits::InspectorIssue> issue) {
  DCHECK_LE(issues_.size(), kMaxIssueCount);
  if (issues_.size() == kMaxIssueCount) {
    issues_.pop_front();
  }
  issues_.emplace_back(frame_tree_node_id, std::move(issue));
}

std::vector<const protocol::Audits::InspectorIssue*>
DevToolsIssueStorage::FilterIssuesBy(
    const base::flat_set<int>& frame_tree_node_ids) const {
  std::vector<const protocol::Audits::InspectorIssue*> issues;
  for (const auto& entry : issues_) {
    if (frame_tree_node_ids.contains(entry.first)) {
      issues.push_back(entry.second.get());
    }
  }
  return issues;
}

void DevToolsIssueStorage::FrameDeleted(int frame_tree_node_id) {
  const int main_frame_id = FrameTreeNode::GloballyFindByID(frame_tree_node_id)
                                ->frame_tree()
                                ->root()
                                ->frame_tree_node_id();
  // Deletion of the main frame causes the DevToolsIssueStorage to be cleaned
  // up. Also there would no longer be a root frame we could re-parent issues
  // on.
  if (frame_tree_node_id == main_frame_id)
    return;

  // Reassign issues from the deleted frame to the root frame.
  for (auto& entry : issues_) {
    if (entry.first == frame_tree_node_id) {
      entry.first = main_frame_id;
    }
  }
}

}  // namespace content
