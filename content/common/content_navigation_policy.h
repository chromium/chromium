// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_
#define CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_

#include "content/common/content_export.h"

#include <array>
#include <string>

namespace content {

CONTENT_EXPORT bool IsBackForwardCacheEnabled();
CONTENT_EXPORT bool IsSameSiteBackForwardCacheEnabled();
CONTENT_EXPORT bool ShouldSkipSameSiteBackForwardCacheForPageWithUnload();
CONTENT_EXPORT bool IsBackForwardCacheDisabledByCommandLine();
CONTENT_EXPORT bool DeviceHasEnoughMemoryForBackForwardCache();

// Whether proactive BrowsingInstance swap can happen on cross-site navigations.
// This can be caused by either the ProactivelySwapBrowsingInstance or the
// BackForwardCache flag.
CONTENT_EXPORT bool CanCrossSiteNavigationsProactivelySwapBrowsingInstances();

// Levels of ProactivelySwapBrowsingInstance support.
// These are additive; features enabled at lower levels remain enabled at all
// higher levels.
enum class ProactivelySwapBrowsingInstanceLevel {
  kDisabled = 0,
  // Swap BrowsingInstance and renderer process on cross-site navigations.
  kCrossSiteSwapProcess = 1,
  // Swap BrowsingInstance on cross-site navigations, but try to reuse the
  // current renderer process if possible.
  kCrossSiteReuseProcess = 2,
  // Swap BrowsingInstance swap on same-site navigations, with process reuse.
  kSameSite = 3,

  kMaxValue = kSameSite,
};

// Levels of ProactivelySwapBrowsingInstance as strings (excluding kDisabled).
CONTENT_EXPORT std::array<
    std::string,
    static_cast<size_t>(ProactivelySwapBrowsingInstanceLevel::kMaxValue)>
ProactivelySwapBrowsingInstanceFeatureEnabledLevelValues();

// Whether the ProactivelySwapBrowsingInstance flag is enabled or not. Will
// return true if the value is set to either of {kCrossSiteSwapProcess,
// kCrossSiteReuseProcess, kSameSite}.
// Note that even if this returns false, we might still trigger proactive
// BrowsingInstance swaps if IsBackForwardCacheEnabled() is true.
CONTENT_EXPORT bool IsProactivelySwapBrowsingInstanceEnabled();

// Whether ProactivelySwapBrowsingInstance with process reuse is enabled or not.
// Will return true if the value is set to either of {kCrossSiteReuseProcess,
// kSameSite}.
CONTENT_EXPORT bool IsProactivelySwapBrowsingInstanceWithProcessReuseEnabled();

// Whether ProactivelySwapBrowsingInstance for same-site navigation is enabled
// or not. Will return true if the value is set to kSameSite.
// Note that even if this returns false, we might still trigger proactive
// BrowsingInstance swaps on same-site navigations if
// IsSameSiteBackForwardCacheEnabled() is true.
CONTENT_EXPORT bool
IsProactivelySwapBrowsingInstanceOnSameSiteNavigationEnabled();

CONTENT_EXPORT extern const char
    kProactivelySwapBrowsingInstanceLevelParameterName[];

// Levels of RenderDocument support. These are additive in that features enabled
// at lower levels remain enabled at all higher levels.
enum class RenderDocumentLevel {
  // Do not reuse RenderFrameHosts when recovering from crashes.
  kCrashedFrame = 1,
  // Also do not reuse RenderFrameHosts when navigating subframes.
  kSubframe = 2,
};
CONTENT_EXPORT bool ShouldCreateNewHostForSameSiteSubframe();
CONTENT_EXPORT RenderDocumentLevel GetRenderDocumentLevel();
CONTENT_EXPORT std::string GetRenderDocumentLevelName(
    RenderDocumentLevel level);
CONTENT_EXPORT extern const char kRenderDocumentLevelParameterName[];

// If this is false we continue the old behaviour of doing an early call to
// RenderFrameHostManager::CommitPending when we are replacing a crashed
// frame.
// TODO(https://crbug.com/1072817): Stop allowing this.
CONTENT_EXPORT bool ShouldSkipEarlyCommitPendingForCrashedFrame();

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_NAVIGATION_POLICY_H_
