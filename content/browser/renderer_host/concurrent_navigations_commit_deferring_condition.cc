// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/concurrent_navigations_commit_deferring_condition.h"

#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"

namespace content {

// static
std::unique_ptr<CommitDeferringCondition>
ConcurrentNavigationsCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request,
    NavigationType navigation_type) {
  // Only defer back/forward cache or prerender activations here. Non-activation
  // navigations are handled by code in NavigationRequest instead.
  if (!navigation_request.IsServedFromBackForwardCache() &&
      navigation_type != NavigationType::kPrerenderedPageActivation) {
    return nullptr;
  }

  // Currently only navigations in the primary main frame can restore pages
  // from BFCache or activate prerendered pages.
  DCHECK(navigation_request.IsInPrimaryMainFrame());

  return base::WrapUnique(
      new ConcurrentNavigationsCommitDeferringCondition(navigation_request));
}

ConcurrentNavigationsCommitDeferringCondition::
    ConcurrentNavigationsCommitDeferringCondition(
        NavigationRequest& navigation_request)
    : CommitDeferringCondition(navigation_request) {}

ConcurrentNavigationsCommitDeferringCondition::
    ~ConcurrentNavigationsCommitDeferringCondition() = default;

CommitDeferringCondition::Result
ConcurrentNavigationsCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  auto* request = NavigationRequest::From(&GetNavigationHandle());

  // See if there is a speculative RFH that has a pending commit cross-document
  // navigation. It's possible for a speculative RenderFrameHost to exist for
  // another navigation as we don't delete existing speculative RFHs that have
  // in-flight commits when we start a new navigation. We need to defer our
  // navigation while the older commit continues, to avoid deleting a pending
  // commit RFH.
  if (request->ShouldQueueDueToExistingPendingCommitRFH()) {
    DCHECK(!request->IsQueued());
    request->set_resume_commit_closure(std::move(resume));
    return Result::kDefer;
  }
  return Result::kProceed;
}

const char* ConcurrentNavigationsCommitDeferringCondition::TraceEventName()
    const {
  return "ConcurrentNavigationsCommitDeferringCondition";
}

}  // namespace content
