// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NO_VARY_SEARCH_HINT_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NO_VARY_SEARCH_HINT_COMMIT_DEFERRING_CONDITION_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/renderer_host/navigation_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/commit_deferring_condition.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

class NavigationRequest;

// This CommitDeferringCondition waits until the prerender navigation receives
// the main page's headers from the server. After receiving the headers,
// we can check if the associated PrerenderHost's initial URL matches via
// No-Vary-Search header in the subsequent CommitDeferringConditions.
class CONTENT_EXPORT PrerenderNoVarySearchHintCommitDeferringCondition
    : public CommitDeferringCondition,
      public PrerenderHost::Observer {
 public:
  ~PrerenderNoVarySearchHintCommitDeferringCondition() override;
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request,
      NavigationType navigation_type,
      std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id);
  Result WillCommitNavigation(base::OnceClosure resume) override;
  const char* TraceEventName() const override;

  // Only used for tests. This task runner is used for precise injection in
  // tests and for timing control.
  static void SetTimerTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  PrerenderNoVarySearchHintCommitDeferringCondition(
      NavigationRequest& navigation_request,
      FrameTreeNodeId candidate_prerender_frame_tree_node_id);
  // PrerenderHost::Observer
  void OnHeadersReceived() override;
  void OnHostDestroyed(PrerenderFinalStatus status) override;

  // Called when `block_until_head_timer_` fires.
  void OnBlockUntilHeadTimerElapsed();
  // Used to set the timer to support testing.
  scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner();

  const FrameTreeNodeId candidate_prerender_frame_tree_node_id_;
  base::OnceClosure resume_;
  // Timer to wait for a configurable amount of time for headers and then
  // give up.
  // This timer cannot outlive this instance, having it as a member variable
  // guarantees it.
  // This timer cannot outlive the PrerenderHost associated either - need to
  // make sure to remove it in PrerenderHost::Observer::OnHostDestroyed.
  // In this way we don't risk blocking the navigation forever.
  std::unique_ptr<base::OneShotTimer> block_until_head_timer_;
  // Keep track if we are waiting on headers during navigation.
  bool waiting_on_headers_ = false;

  // Used to enable injection of a task runner for precise timing control
  // in tests.
  static scoped_refptr<base::SingleThreadTaskRunner>&
  GetTimerTaskRunnerForTesting();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NO_VARY_SEARCH_HINT_COMMIT_DEFERRING_CONDITION_H_
