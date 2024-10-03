// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CONCURRENT_NAVIGATIONS_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_RENDERER_HOST_CONCURRENT_NAVIGATIONS_COMMIT_DEFERRING_CONDITION_H_

#include <memory>

#include "content/public/browser/commit_deferring_condition.h"

namespace content {

class NavigationRequest;

// Defers/queues activations to back-forward cached or prerendered pages when
// there are pending commit RenderFrameHosts used by other navigations. This
// ensures that the activations won't delete pending commit RenderFrameHosts.
// Once there is no pending commit RenderFrameHost, the activation can be
// resumed. This is triggered from the NavigationRequest destructor of another
// navigation. See also `NavigationRequest::ResumeCommitIfNeeded()` and its
// callers.
//
// Note that there is also a slightly similar queueing operation done for
// non-activation navigations when trying to pick the final RenderFrameHost for
// the navigation. Both that and this deferring condition uses a similar
// resuming sequence, but different entry points to the deferred/queued states.
// This is OK because both methods cover mutually exclusive cases (this
// deferring condition covers activations only, and the queueing logic within
// NavigationRequest covers non-activations only). For more details, see also
// `NavigationRequest::SelectFrameHostForOnResponseStarted()` and other related
// functions.
class ConcurrentNavigationsCommitDeferringCondition
    : public CommitDeferringCondition {
 public:
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request,
      NavigationType navigation_type);

  ConcurrentNavigationsCommitDeferringCondition(
      const ConcurrentNavigationsCommitDeferringCondition&) = delete;
  ConcurrentNavigationsCommitDeferringCondition& operator=(
      const ConcurrentNavigationsCommitDeferringCondition&) = delete;

  ~ConcurrentNavigationsCommitDeferringCondition() override;

  Result WillCommitNavigation(base::OnceClosure resume) override;
  const char* TraceEventName() const override;

 private:
  explicit ConcurrentNavigationsCommitDeferringCondition(
      NavigationRequest& navigation_request);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CONCURRENT_NAVIGATIONS_COMMIT_DEFERRING_CONDITION_H_
