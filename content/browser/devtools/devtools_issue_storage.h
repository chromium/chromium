// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_ISSUE_STORAGE_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_ISSUE_STORAGE_H_

#include "base/containers/circular_deque.h"
#include "base/unguessable_token.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

namespace protocol {
namespace Audits {
class InspectorIssue;
}  // namespace Audits
}  // namespace protocol

// TODO(crbug.com/40051801): Attribute issues to ongoing navigations correctly.
class DevToolsIssueStorage
    : public content::PageUserData<DevToolsIssueStorage> {
 public:
  ~DevToolsIssueStorage() override;

  void AddInspectorIssue(
      RenderFrameHost* render_frame_host,
      std::unique_ptr<protocol::Audits::InspectorIssue> issue);
  std::vector<const protocol::Audits::InspectorIssue*> FindIssuesForAgentOf(
      RenderFrameHost* render_frame_host) const;

 private:
  explicit DevToolsIssueStorage(Page& page);
  friend class content::PageUserData<DevToolsIssueStorage>;
  PAGE_USER_DATA_KEY_DECL();

  using RenderFrameHostAssociatedIssue =
      std::pair<GlobalRenderFrameHostId,
                std::unique_ptr<protocol::Audits::InspectorIssue>>;
  base::circular_deque<RenderFrameHostAssociatedIssue> issues_;
  int total_added_issues_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_ISSUE_STORAGE_H_
