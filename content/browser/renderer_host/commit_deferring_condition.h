// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_H_

#include "base/callback.h"

namespace content {

// Base class allowing clients to defer an activation or a navigation that's
// ready to commit. See commit_deferring_condition_runner.h for more details.
class CommitDeferringCondition {
 public:
  enum class NavigationType {
    kPrerenderedPageActivation,

    // Other navigations including same-document navigations and restores from
    // BackForwardCache.
    // TODO(https://crbug.com/1226442): Split this into kBackForwardCache and
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
    kDefer
  };

  CommitDeferringCondition() = default;
  virtual ~CommitDeferringCondition() = default;

  // Override to check if the navigation should be allowed to commit or it
  // should be deferred. If this method returns true, this condition is
  // already satisfied and the navigation should be allowed to commit. If it
  // returns false, the condition will call |resume| asynchronously to
  // indicate completion.
  virtual Result WillCommitNavigation(base::OnceClosure resume) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_H_
