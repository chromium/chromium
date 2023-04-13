// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_commit_deferring_condition.h"

#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"

namespace content {

// static
std::unique_ptr<CommitDeferringCondition>
BackForwardCacheCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request) {
  if (!navigation_request.IsServedFromBackForwardCache())
    return nullptr;

  // Currently only navigations in the primary main frame can restore pages
  // from BFCache.
  DCHECK(navigation_request.IsInPrimaryMainFrame());

  return base::WrapUnique(
      new BackForwardCacheCommitDeferringCondition(navigation_request));
}

BackForwardCacheCommitDeferringCondition::
    BackForwardCacheCommitDeferringCondition(
        NavigationRequest& navigation_request)
    : CommitDeferringCondition(navigation_request) {}

BackForwardCacheCommitDeferringCondition::
    ~BackForwardCacheCommitDeferringCondition() = default;

CommitDeferringCondition::Result
BackForwardCacheCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  DCHECK(GetNavigationHandle().IsServedFromBackForwardCache());

  BackForwardCacheImpl& bfcache =
      NavigationRequest::From(&GetNavigationHandle())
          ->frame_tree_node()
          ->navigator()
          .controller()
          .GetBackForwardCache();

  // If an entry doesn't exist (it was evicted?) there's no need to defer the
  // commit as we'll end up performing a new navigation.
  auto bfcache_entry = bfcache.GetOrEvictEntry(
      NavigationRequest::From(&GetNavigationHandle())->nav_entry_id());
  // TODO(crbug.com/1430653): Check the
  // `BackForwardCacheImpl::GetEntryFailureCase` in the return value and cancel
  // the NavigationRequest to avoid use-after-free if we know that it will be
  // restarted.
  if (!bfcache_entry.has_value()) {
    return Result::kProceed;
  }

  bfcache.WillCommitNavigationToCachedEntry(*(bfcache_entry.value()),
                                            std::move(resume));
  return Result::kDefer;
}

}  // namespace content
