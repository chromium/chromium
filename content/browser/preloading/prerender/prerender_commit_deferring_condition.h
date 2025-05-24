// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_COMMIT_DEFERRING_CONDITION_H_

#include <memory>

#include "base/time/time.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class NavigationRequest;

// PrerenderCommitDeferringCondition defers a prerender activation with ongoing
// main frame navigation in prerender frame tree. When this happens we wait for
// the ongoing main frame navigation to commit before resuming the prerender
// activation.
//
// This implementation assumes that there are no new navigations happening in
// the main frame during prerender activation i.e., between the ongoing
// navigation commit until the prerender activates.
class PrerenderCommitDeferringCondition : public CommitDeferringCondition,
                                          public WebContentsObserver {
 public:
  ~PrerenderCommitDeferringCondition() override;

  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request,
      NavigationType navigation_type,
      std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id);

  Result WillCommitNavigation(base::OnceClosure resume) override;
  const char* TraceEventName() const override;

 private:
  PrerenderCommitDeferringCondition(
      NavigationRequest& navigation_request,
      FrameTreeNodeId candidate_prerender_frame_tree_node_id);

  // WebContentsObserver
  // Tracks the ongoing navigation commit in prerender frame tree to resume the
  // activation.
  void DidFinishNavigation(NavigationHandle* handle) override;

  // The root frame tree node id of the prerendered page that this navigation
  // will attempt to activate. See comments on
  // `CommitDeferringConditionRunner::candidate_prerender_frame_tree_node_id_`
  // for details.
  const FrameTreeNodeId candidate_prerender_frame_tree_node_id_;

  // The time PrerenderCommitDeferringCondition started deferring the
  // navigation.
  base::TimeTicks defer_start_time_;

  base::OnceClosure done_closure_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_COMMIT_DEFERRING_CONDITION_H_
