// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PROCESS_SELECTION_DEFERRING_CONDITION_H_
#define CONTENT_PUBLIC_BROWSER_PROCESS_SELECTION_DEFERRING_CONDITION_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

class NavigationHandle;
class ProcessSelectionUserData;

// Base class to allow clients to prepare inputs that are needed for selecting a
// process during a navigation request, optionally deferring the final process
// selection when necessary. On creation, the implementing class can begin to
// prepare results for the associated NavigationHandle by queuing asynchronous
// tasks. The `ProcessSelectionDeferringCondition` will be notified at
// `NavigationRequest::OnRequestRedirected()` time, giving the class an
// opportunity to schedule additional tasks, or cancel previously queued ones.
// Then, after the network response is received, the navigation will call
// `OnWillSelectFinalProcess()`. At this time, the class can defer by returning
// `Result::kDefer`. If it returns `Result::kDefer`, then it must call the
// provided resume closure in order for the navigation to proceed. Once all
// conditions have resumed, navigation continues from
// `NavigationRequest::SelectFrameHostForOnResponseStarted()`.
//
// `ProcessSelectionDeferringCondition` vs. `NavigationThrottle` and
// `CommitDeferringCondition`: It may seem like either a `NavigationThrottle` or
// a `CommitDeferringCondition` could be used to provide the same functionality
// by deferring in `NavigationThrottle::WillProcessResponse` or
// `CommitDeferringCondition::WillCommitNavigation`, however those hooks occur
// after process selection has been finalized. Another key difference between
// these mechanisms is that `ProcessSelectionDeferringCondition` can only defer
// and is unable to cancel or otherwise influence the navigation request. Most
// potential clients will want to use the `NavigationThrottle` or the
// `CommitDeferringCondition` mechanism for their use-case.
class CONTENT_EXPORT ProcessSelectionDeferringCondition {
 public:
  enum class Result {
    // Returned when the condition is satisfied and the client can synchronously
    // proceed with process selection.
    kProceed,
    // Returned when the condition needs to asynchronously wait before allowing
    // process selection to occur.
    kDefer,
  };

  ProcessSelectionDeferringCondition() = delete;
  explicit ProcessSelectionDeferringCondition(
      NavigationHandle& navigation_handle);
  ProcessSelectionDeferringCondition(
      const ProcessSelectionDeferringCondition& other) = delete;
  ProcessSelectionDeferringCondition& operator=(
      const ProcessSelectionDeferringCondition& other) = delete;
  virtual ~ProcessSelectionDeferringCondition();

  // Called when the navigation request is redirected. This should handle
  // canceling any currently running tasks that are no longer needed, as well as
  // starting new tasks for the redirected request.
  virtual void OnRequestRedirected() {}

  // Called when the navigation request will select the renderer host process.
  // If the condition is ready, return `Result::kProceed` so that the navigation
  // can continue with selecting a final SiteInstance and process; otherwise
  // return `Result::kDefer`. When `kDefer` is returned, the navigation will
  // block until the passed in resume closure is called. NOTE: Avoid deferring
  // whenever possible because this slows page loads. See the comment in
  // NavigationThrottle::ThrottleAction::DEFER for more information.
  virtual Result OnWillSelectFinalProcess(base::OnceClosure resume) = 0;

  NavigationHandle& navigation_handle() const { return *navigation_handle_; }

  // A convenience method to get the ProcessSelectionUserData instance
  // associated with this navigation. Implementers of this class can use this
  // data container to attach any relevant information that needs to be made
  // available to the process selection logic. See `ProcessSelectionUserData`
  // for details about defining and retrieving custom data.
  ProcessSelectionUserData& GetProcessSelectionUserData() {
    return navigation_handle_->GetProcessSelectionUserData();
  }

 private:
  base::SafeRef<NavigationHandle> navigation_handle_;
};
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PROCESS_SELECTION_DEFERRING_CONDITION_H_
