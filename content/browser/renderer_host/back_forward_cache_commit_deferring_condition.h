// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_COMMIT_DEFERRING_CONDITION_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/commit_deferring_condition.h"

namespace content {

class NavigationRequest;

// Defers a navigation commit to a back-forward cached page. When activating a
// page from the back forward cache, we disable JS eviction and unfreeze the
// page before committing it.  When this happens we need to wait for the
// renderer to respond that eviction is disabled before we proceed. This
// condition is used to defer committing until we hear back from all renderers.
class BackForwardCacheCommitDeferringCondition
    : public CommitDeferringCondition {
 public:
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request);

  ~BackForwardCacheCommitDeferringCondition() override;

  Result WillCommitNavigation(base::OnceClosure resume) override;

 private:
  explicit BackForwardCacheCommitDeferringCondition(
      NavigationRequest& navigation_request);

  NavigationRequest& navigation_request_;

  DISALLOW_COPY_AND_ASSIGN(BackForwardCacheCommitDeferringCondition);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_COMMIT_DEFERRING_CONDITION_H_
