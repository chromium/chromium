// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_API_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_API_COMMIT_DEFERRING_CONDITION_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"

namespace blink {
struct ViewTransitionState;
}

namespace content {
class NavigationRequest;
class RenderFrameHostImpl;
class ScopedViewTransitionResources;

// This commit deferring condition implements renderer-side commit deferring,
// which can be triggered via the navigation API.
// See explainer:
// https://github.com/w3c/csswg-drafts/blob/main/css-view-transitions-2/two-phase-transition-explainer.md#solution-1-allowing-the-author-to-control-the-commit-scheduling
// A "resume listener" is passed from the renderer, and is triggered by the
// navigation API when the commit can proceed.
class CONTENT_EXPORT NavigationAPICommitDeferringCondition
    : public CommitDeferringCondition,
      public blink::mojom::NavigationResumeDeferredCommitListener {
 public:
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest&);

  NavigationAPICommitDeferringCondition(
      const NavigationAPICommitDeferringCondition&) = delete;
  NavigationAPICommitDeferringCondition& operator=(
      const NavigationAPICommitDeferringCondition&) = delete;

  ~NavigationAPICommitDeferringCondition() override;

  Result WillCommitNavigation(base::OnceClosure resume) override;
  const char* TraceEventName() const override;
  void ResumeDeferredCommit() override;

 private:
  explicit NavigationAPICommitDeferringCondition(NavigationRequest&);
  base::OnceClosure resume_navigation_;
  mojo::Receiver<blink::mojom::NavigationResumeDeferredCommitListener>
      resume_listener_;
  bool resumed_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_API_COMMIT_DEFERRING_CONDITION_H_
