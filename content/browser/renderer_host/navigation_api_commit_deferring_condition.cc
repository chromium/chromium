// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_api_commit_deferring_condition.h"

#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

// static
std::unique_ptr<CommitDeferringCondition>
NavigationAPICommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request) {
  // The navigation API has not deferred the commit, bail.
  if (!navigation_request.HasResumeAfterDeferredCommitListener()) {
    return nullptr;
  }

  // This feature is not supported on fenced frames or prerendering.
  if (navigation_request.frame_tree_node()->frame_tree().type() !=
      FrameTree::Type::kPrimary) {
    return nullptr;
  }

  // Commit deferring is only available for same-origin navigations.
  // This check is also done in the renderer, but double check here in case
  // the request is compromised.
  if (navigation_request.frame_tree_node()
          ->current_frame_host()
          ->GetLastCommittedOrigin() !=
      *navigation_request.GetOriginToCommit()) {
    mojo::ReportBadMessage(
        "Navigation API deferring condition is not allowed in cross-origin "
        "navigations");
    return nullptr;
  }

  // TODO(nrosenthal): perhaps bail from deferring if the navigation encountered
  // a cross-origin redirect?
  return base::WrapUnique(
      new NavigationAPICommitDeferringCondition(navigation_request));
}

NavigationAPICommitDeferringCondition::
    ~NavigationAPICommitDeferringCondition() = default;

void NavigationAPICommitDeferringCondition::ResumeDeferredCommit() {
  resumed_ = true;
  // resume_navigation_ would have a value if the defer() promises are not
  // resolved yet, but the navigation is ready to commit.
  if (resume_navigation_) {
    std::move(resume_navigation_).Run();
  }
}

NavigationAPICommitDeferringCondition::NavigationAPICommitDeferringCondition(
    NavigationRequest& navigation_request)
    : CommitDeferringCondition(navigation_request),
      resume_listener_(
          this,
          navigation_request.TakeResumeAfterDeferredCommitListener()) {}

CommitDeferringCondition::Result
NavigationAPICommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  // resumed_ would be true if the defer() promises are resolved
  // before the navigation is ready to commit.
  if (resumed_) {
    return Result::kProceed;
  }

  resume_navigation_ = std::move(resume);
  return Result::kDefer;
}

const char* NavigationAPICommitDeferringCondition::TraceEventName() const {
  return "NavigationAPICommitDeferringCondition";
}

}  // namespace content
