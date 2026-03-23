// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/async_before_unload_commit_deferring_condition.h"

#include "content/browser/renderer_host/navigation_request.h"

namespace content {

// static
std::unique_ptr<CommitDeferringCondition>
AsyncBeforeUnloadCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request) {
  if (!navigation_request.IsWaitingForAsyncBeforeUnload()) {
    return nullptr;
  }

  TRACE_EVENT("navigation",
              "AsyncBeforeUnloadCommitDeferringCondition::MaybeCreate");

  return base::WrapUnique(
      new AsyncBeforeUnloadCommitDeferringCondition(navigation_request));
}

AsyncBeforeUnloadCommitDeferringCondition::
    AsyncBeforeUnloadCommitDeferringCondition(
        NavigationRequest& navigation_request)
    : CommitDeferringCondition(navigation_request) {}

AsyncBeforeUnloadCommitDeferringCondition::
    ~AsyncBeforeUnloadCommitDeferringCondition() = default;

CommitDeferringCondition::Result
AsyncBeforeUnloadCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure commit_resume_closure) {
  TRACE_EVENT(
      "navigation",
      "AsyncBeforeUnloadCommitDeferringCondition::WillCommitNavigation");

  NavigationRequest& navigation_request =
      *NavigationRequest::From(&GetNavigationHandle());

  if (!navigation_request.IsWaitingForAsyncBeforeUnload()) {
    return Result::kProceed;
  }

  // Register the callback to be invoked when the renderer completes its
  // beforeunload execution or when the timeout occurs.
  navigation_request.SetAsyncBeforeUnloadCommitResumeClosure(
      std::move(commit_resume_closure));

  return Result::kDefer;
}

const char* AsyncBeforeUnloadCommitDeferringCondition::TraceEventName() const {
  return "AsyncBeforeUnloadCommitDeferringCondition";
}

}  // namespace content
