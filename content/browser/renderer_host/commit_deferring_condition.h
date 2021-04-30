// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_H_

#include "base/callback.h"

namespace content {

// Base class allowing clients to defer a navigation that's ready to commit.
// See commit_deferring_condition_runner.h for more details.
class CommitDeferringCondition {
 public:
  CommitDeferringCondition() = default;
  virtual ~CommitDeferringCondition() = default;

  // Override to check if the navigation should be allowed to commit or it
  // should be deferred. If this method returns true, this condition is
  // already satisfied and the navigation should be allowed to commit. If it
  // returns false, the condition will call |resume| asynchronously to
  // indicate completion.
  virtual bool WillCommitNavigation(base::OnceClosure resume) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_H_
