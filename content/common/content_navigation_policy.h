// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_
#define CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_

#include "base/feature_list.h"
#include "content/common/content_export.h"

#include <array>
#include <string>

namespace features {

// The BackForwardCache_NoMemoryLimit_Trial feature flag's sole purpose is to
// make it possible to get a group for "all devices except when BackForwardCache
// feature is specifically disabled due to non-memory-control reasons". This is
// done by querying the flag if and only if the device satisifes one of the
// following:
// 1) The device does not have enough memory for BackForwardCache, or
// 2) The device has enough memory and the BackForwardCache feature is enabled.
// With that, we will include the devices that don't have enough memory while
// avoiding activating the BackForwardCache experiment, and won’t include
// devices that do have enough memory but have the BackForwardCache flag
// disabled.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCache_NoMemoryLimit_Trial);
}  // namespace features

namespace content {

// Returns whether the BackForwardCache is enabled or not according to the
// feature flags. Note that even if this returns true, the embedder might still
// disable BackForwardCache by returning false in
// `WebContentsDelegate::IsBackForwardCacheSupported()`, which this function
// does not consider.
CONTENT_EXPORT bool IsBackForwardCacheEnabled();

CONTENT_EXPORT bool IsBackForwardCacheDisabledByCommandLine();
CONTENT_EXPORT bool DeviceHasEnoughMemoryForBackForwardCache();

// Whether proactive BrowsingInstance swap can happen on cross-site navigations.
// This can be caused by the BackForwardCache flag.
CONTENT_EXPORT bool CanCrossSiteNavigationsProactivelySwapBrowsingInstances();

// Levels of RenderDocument support. These are additive in that features enabled
// at lower levels remain enabled at all higher levels.
enum class RenderDocumentLevel {
  // Do not reuse RenderFrameHosts when recovering from crashes.
  kCrashedFrame = 1,
  // Do not reuse RenderFrameHosts when navigating non-local-root subframes.
  kNonLocalRootSubframe = 2,
  // Do not reuse RenderFrameHosts when navigating any subframe.
  kSubframe = 3,
  // Do not reuse RenderFrameHosts when navigating any frame.
  kAllFrames = 4,
};

// Whether same-site navigations will result in a change of RenderFrameHosts,
// which will happen when RenderDocument is enabled. Due to the various levels
// of the feature, the result may differ depending on whether the
// RenderFrameHost is a main/local root/non-local-root frame, whether it has
// committed any navigations or not, and whether it's a crashed frame that
// must be replaced or not.
CONTENT_EXPORT bool ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
    bool is_main_frame,
    bool is_local_root = true,
    bool has_committed_any_navigation = true,
    bool must_be_replaced = false);
CONTENT_EXPORT bool ShouldCreateNewHostForAllFrames();
CONTENT_EXPORT RenderDocumentLevel GetRenderDocumentLevel();
CONTENT_EXPORT std::string GetRenderDocumentLevelName(
    RenderDocumentLevel level);
CONTENT_EXPORT extern const char kRenderDocumentLevelParameterName[];

// If this is false we continue the old behaviour of doing an early call to
// RenderFrameHostManager::CommitPending when we are replacing a crashed
// frame.
// TODO(crbug.com/40052076): Stop allowing this.
CONTENT_EXPORT bool ShouldSkipEarlyCommitPendingForCrashedFrame();

// The levels for the kQueueNavigationsWhileWaitingForCommit feature.
enum class NavigationQueueingFeatureLevel {
  // Feature is disabled.
  kNone,
  // Navigation code attempts to avoid unnecessary cancellations; otherwise,
  // queueing navigations is pointless because the slow-to-commit page will
  // simply cancel the queued navigation request.
  kAvoidRedundantCancellations,
  // Navigation code attempts to queue navigations rather than clobbering a
  // speculative RenderFrameHost that is waiting for the renderer to acknowledge
  // the navigation commit.
  kFull,
};

CONTENT_EXPORT NavigationQueueingFeatureLevel
GetNavigationQueueingFeatureLevel();

// Returns true if GetNavigationQueueingFeatureLevel() returns at least
// kAvoidRedundantCancellations.
CONTENT_EXPORT bool ShouldAvoidRedundantNavigationCancellations();

// Returns true if GetNavigationQueueingFeatureLevel() is kFull.
CONTENT_EXPORT bool ShouldQueueNavigationsWhenPendingCommitRFHExists();

// Returns true if data: URL subframes should be put in a separate SiteInstance
// in the SiteInstanceGroup of the initiator.
CONTENT_EXPORT bool ShouldCreateSiteInstanceForDataUrls();
}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_
