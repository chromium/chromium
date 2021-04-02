// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_ISSUE_STORAGE_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_ISSUE_STORAGE_H_

#include "base/containers/circular_deque.h"
#include "base/unguessable_token.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

namespace protocol {
namespace Audits {
class InspectorIssue;
}  // namespace Audits
}  // namespace protocol

// TODO(crbug.com/1063007): Attribute issues to ongoing navigations correctly.
// TODO(crbug.com/1090679): Replace RenderDocumentHostUserData with
//                          PageUserData.
class DevToolsIssueStorage
    : public content::RenderDocumentHostUserData<DevToolsIssueStorage>,
      public WebContentsObserver {
 public:
  ~DevToolsIssueStorage() override;

  void AddInspectorIssue(
      int frame_tree_node_id,
      std::unique_ptr<protocol::Audits::InspectorIssue> issue);
  std::vector<const protocol::Audits::InspectorIssue*> FilterIssuesBy(
      const base::flat_set<int>& frame_tree_node_ids) const;

 private:
  explicit DevToolsIssueStorage(RenderFrameHost* rfh);
  friend class content::RenderDocumentHostUserData<DevToolsIssueStorage>;
  RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();

  // WebContentsObserver overrides.
  void FrameDeleted(int frame_tree_node_id) override;

  using FrameAssociatedIssue =
      std::pair<int, std::unique_ptr<protocol::Audits::InspectorIssue>>;
  base::circular_deque<FrameAssociatedIssue> issues_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_ISSUE_STORAGE_H_
