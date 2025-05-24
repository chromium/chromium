// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VIEW_TRANSITION_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_RENDERER_HOST_VIEW_TRANSITION_COMMIT_DEFERRING_CONDITION_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/commit_deferring_condition.h"

namespace blink {
struct ViewTransitionState;
}

namespace content {
class NavigationRequest;
class RenderFrameHostImpl;
class ScopedViewTransitionResources;

class CONTENT_EXPORT ViewTransitionCommitDeferringCondition
    : public CommitDeferringCondition {
 public:
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request);

  ViewTransitionCommitDeferringCondition(
      const ViewTransitionCommitDeferringCondition&) = delete;
  ViewTransitionCommitDeferringCondition& operator=(
      const ViewTransitionCommitDeferringCondition&) = delete;

  ~ViewTransitionCommitDeferringCondition() override;

  Result WillCommitNavigation(base::OnceClosure resume) override;
  const char* TraceEventName() const override;

 private:
  explicit ViewTransitionCommitDeferringCondition(
      NavigationRequest& navigation_request);

  void OnSnapshotAckFromRenderer(
      const blink::ViewTransitionState& view_transition_state);
  void OnSnapshotTimeout();
  base::TimeDelta GetSnapshotCallbackTimeout() const;

  std::unique_ptr<ScopedViewTransitionResources> resources_;
  base::WeakPtr<RenderFrameHostImpl> old_rfh_;
  base::OnceClosure resume_navigation_;
  base::WeakPtrFactory<ViewTransitionCommitDeferringCondition> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_VIEW_TRANSITION_COMMIT_DEFERRING_CONDITION_H_
