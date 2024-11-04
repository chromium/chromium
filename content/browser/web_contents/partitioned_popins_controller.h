// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_PARTITIONED_POPINS_CONTROLLER_H_
#define CONTENT_BROWSER_WEB_CONTENTS_PARTITIONED_POPINS_CONTROLLER_H_

#include "base/types/pass_key.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

// Controls the lifecycle of Partitioned Popins by listening to their opener's
// WebContents' events.
class PartitionedPopinsController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PartitionedPopinsController> {
 public:
  using PassKey = base::PassKey<PartitionedPopinsController>;

  PartitionedPopinsController(const PartitionedPopinsController&) = delete;
  PartitionedPopinsController& operator=(const PartitionedPopinsController&) =
      delete;
  ~PartitionedPopinsController() override;

  // content::WebContentsObserver
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(FrameTreeNodeId frame_tree_node_id) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(Page& page) override;

 private:
  explicit PartitionedPopinsController(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PartitionedPopinsController>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_PARTITIONED_POPINS_CONTROLLER_H_
