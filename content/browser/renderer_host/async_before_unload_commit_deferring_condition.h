// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ASYNC_BEFORE_UNLOAD_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_RENDERER_HOST_ASYNC_BEFORE_UNLOAD_COMMIT_DEFERRING_CONDITION_H_

#include "base/timer/timer.h"
#include "content/public/browser/commit_deferring_condition.h"

namespace content {

class NavigationRequest;

// AsyncBeforeUnloadCommitDeferringCondition defers the navigation commit until
// asynchronously running beforeunload handlers have completed.
//
// Even though these handlers cannot show a confirmation dialog or cancel the
// navigation (due to the lack of user activation), this condition ensures we
// still wait for them to finish before allowing the new page to commit.
//
// This is necessary to prevent the risk of "observable actions" (e.g., state
// saving to localStorage) being attempted after the next page has already
// committed. Without this deferral, a page in a "pending deletion" state could
// perform actions that conflict with the new page's state or lead to
// unpredictable race conditions.
class AsyncBeforeUnloadCommitDeferringCondition
    : public CommitDeferringCondition {
 public:
  // Creates a condition if the navigation is running beforeunload handlers
  // asynchronously.
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request);
  ~AsyncBeforeUnloadCommitDeferringCondition() override;

  // CommitDeferringCondition:
  Result WillCommitNavigation(base::OnceClosure resume) override;
  const char* TraceEventName() const override;

 private:
  explicit AsyncBeforeUnloadCommitDeferringCondition(
      NavigationRequest& navigation_request);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ASYNC_BEFORE_UNLOAD_COMMIT_DEFERRING_CONDITION_H_
