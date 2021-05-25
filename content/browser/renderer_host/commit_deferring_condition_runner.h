// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_RUNNER_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_RUNNER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {

class CommitDeferringCondition;
class NavigationRequest;

// Helper class used to defer an otherwise fully-prepared navigation (i.e.
// followed all redirects, passed all NavigationThrottle checks) from
// preceding until all preconditions are met.
//
// Clients subclass the CommitDeferringCondition class to wait on a commit
// blocking condition to be resolved and invoke the callback when it's ready.
// The client should register their subclass class in
// RegisterDeferringConditions().  Each condition is run in order, waiting on
// that condition to call Resume() before starting the next one. Once the final
// condition is completed, the navigation is resumed to commit.
//
// This mechanism is not applied to about:blank or same-document navigations.
//
// CommitDeferringCondition vs. NavigationThrottle: At first glance this
// mechanism may seem redundant to using a NavigationThrottle (and deferring in
// WillProcessResponse). However, the behavior will differ on pages initially
// loaded into a non-primary FrameTree (e.g. prerendering or BFCached page).
// In these cases NavigationThrottles will run only when the page was loading,
// they will not get a chance to intervene when it is being activated to the
// primary FrameTree (i.e. user navigates to a prerendered page). If the
// navigation needs to be deferred during such activations, a
// CommitDeferringCondition must be used.  It runs both when the navigation is
// loading and when a navigation activates into the primary FrameTree.
class CONTENT_EXPORT CommitDeferringConditionRunner {
 public:
  class Delegate {
   public:
    virtual void OnCommitDeferringConditionChecksComplete() = 0;
  };

  // Creates the runner and adds all the conditions in
  // RegisterDeferringConditions.
  static std::unique_ptr<CommitDeferringConditionRunner> Create(
      NavigationRequest& navigation_request);

  ~CommitDeferringConditionRunner();

  // Call to start iterating through registered CommitDeferringConditions. This
  // calls OnCommitDeferringConditionChecksComplete on the |delegate_| when all
  // conditions have been resolved. This may happen either synchronously or
  // asynchronously.
  void ProcessChecks();

  // Call to register all deferring conditions. This should be called when
  // NavigationState >= WILL_START_REQUEST.
  void RegisterDeferringConditions(NavigationRequest& navigation_request);

  // Used in tests to inject mock conditions.
  void AddConditionForTesting(
      std::unique_ptr<CommitDeferringCondition> condition);

  // Used in tests to check if CommitDeferringConditionRunner is currently
  // deferred for the navigation or not.
  bool is_deferred_for_testing() const;

 private:
  friend class CommitDeferringConditionRunnerTest;

  explicit CommitDeferringConditionRunner(Delegate& delegate);

  // Called asynchronously to resume iterating through
  // CommitDeferringConditions after one has been deferred. A callback for this
  // method is passed into each condition when WillCommitNavigation is called.
  void ResumeProcessing();

  void ProcessConditions();
  void AddCondition(std::unique_ptr<CommitDeferringCondition> condition);

  std::vector<std::unique_ptr<CommitDeferringCondition>> conditions_;

  // This class is owned by its delegate (the NavigationRequest) so it's safe
  // to keep a reference to it.
  Delegate& delegate_;

  // True when we're blocked waiting on a call to ResumeProcessing.
  bool is_deferred_ = false;

  base::WeakPtrFactory<CommitDeferringConditionRunner> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CommitDeferringConditionRunner);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_RUNNER_H_
