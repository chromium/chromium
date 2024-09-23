// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SHOULD_SWAP_BROWSING_INSTANCE_H_
#define CONTENT_BROWSER_RENDERER_HOST_SHOULD_SWAP_BROWSING_INSTANCE_H_

namespace content {

// This is the enumeration of the reasons why we might not swap the
// BrowsingInstance for navigations.
// This enum is used for histograms and should not be renumbered.
// TODO(crbug.com/40108107): Remove after the investigations are complete.
enum class ShouldSwapBrowsingInstance {
  kYes_ForceSwap = 0,
  kNo_ProactiveSwapDisabled = 1,
  kNo_NotMainFrame = 2,
  kNo_HasRelatedActiveContents = 3,
  kNo_DoesNotHaveSite = 4,
  kNo_SourceURLSchemeIsNotHTTPOrHTTPS = 5,
  // 6: kNo_DestinationURLSchemeIsNotHTTPOrHTTPS was removed as the scheme of
  // the destination URL should not affect back-forward cache eligibility, so
  // we don't need to avoid doing a proactive BrowsingInstance swap due to it.
  kNo_SameSiteNavigation = 7,
  // 8: kNo_ReloadingErrorPage was removed as the special case that forced
  // reusing a SiteInstance for auto-reload was fixed. (see
  // https://crbug.com/1045524).
  kNo_AlreadyHasMatchingBrowsingInstance = 9,
  kNo_RendererDebugURL = 10,
  kNo_NotNeededForBackForwardCache = 11,
  kYes_CrossSiteProactiveSwap = 12,
  kYes_SameSiteProactiveSwap = 13,
  kNo_SameDocumentNavigation = 14,
  kNo_SameUrlNavigation = 15,
  kNo_WillReplaceEntry = 16,
  kNo_Reload = 17,
  kNo_Guest = 18,
  kNo_HasNotComittedAnyNavigation = 19,
  // 20: kNo_UnloadHandlerExistsOnSameSiteNavigation was removed as it's not
  // triggering BrowsingInstance swap anymore. See
  // https://groups.google.com/a/google.com/g/chrome-bfcache/c/L-ZreZDY4n0
  kNo_NotPrimaryMainFrame = 21,
  kNo_InitiatorRequestedNoProactiveSwap = 22,

  kMaxValue = kNo_InitiatorRequestedNoProactiveSwap
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SHOULD_SWAP_BROWSING_INSTANCE_H_
