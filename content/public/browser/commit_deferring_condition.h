// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_PUBLIC_BROWSER_COMMIT_DEFERRING_CONDITION_H_

#include "base/functional/callback.h"

#include "base/memory/safe_ref.h"
#include "content/common/content_export.h"

namespace content {

class NavigationHandle;

// Base class allowing clients to defer an activation or a navigation that's
// ready to commit. See commit_deferring_condition_runner.h for more details.
class CONTENT_EXPORT CommitDeferringCondition {
 public:
  enum class NavigationType {
    kPrerenderedPageActivation,

    // Other navigations including same-document navigations and restores from
    // BackForwardCache.
    // TODO(crbug.com/40188852): Split this into kBackForwardCache and
    // kNewDocumentLoad.
    kOther,
  };

  enum class Result {
    // Returned when the condition is satisfied and the client can
    // synchronously proceed to commit the navigation.
    kProceed,
    // Returned when the condition needs to asynchronously wait before allowing
    // a commit. If this is returned, the condition will invoke the passed in
    // |resume| closure when it is ready.
    // Note: see comment in NavigationThrottle::ThrottleAction::DEFER about
    // avoiding deferring if possible due to performance degradations.
    kDefer,
    // Returned when it is known that the navigation has been cancelled and we
    // should not proceed to commit it to avoid user-after-free.
    kCancelled,
  };

  CommitDeferringCondition() = delete;
  explicit CommitDeferringCondition(NavigationHandle& navigation_handle);
  virtual ~CommitDeferringCondition();

  // Override to check if the navigation should be allowed to commit or it
  // should be deferred. If this method returns true, this condition is
  // already satisfied and the navigation should be allowed to commit. If it
  // returns false, the condition will call |resume| asynchronously to
  // indicate completion.
  virtual Result WillCommitNavigation(base::OnceClosure resume) = 0;

  // Name used in tracing. Usually the same as the derived class name.
  virtual const char* TraceEventName() const = 0;

  NavigationHandle& GetNavigationHandle() const { return *navigation_handle_; }

 private:
  base::SafeRef<NavigationHandle> navigation_handle_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COMMIT_DEFERRING_CONDITION_H_
