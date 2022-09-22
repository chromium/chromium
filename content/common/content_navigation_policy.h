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

CONTENT_EXPORT bool IsBackForwardCacheEnabled();
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
// IsBackForwardCacheEnabled() is true.
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
  // Do not reuse RenderFrameHosts when navigating any frame.
  kAllFrames = 3,
};
CONTENT_EXPORT bool ShouldCreateNewHostForSameSiteSubframe();
CONTENT_EXPORT bool ShouldCreateNewHostForAllFrames();
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
