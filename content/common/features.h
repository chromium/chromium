// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FEATURES_H_
#define CONTENT_COMMON_FEATURES_H_

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "content/common/content_export.h"

namespace content {

// Please keep features in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
// Enables ADPF (Android Dynamic Performance Framework) for the browser IO
// thread.
BASE_DECLARE_FEATURE(kADPFForBrowserIOThread);

// Unifies RenderWidgetHostViewAndroid with the other platforms in their usage
// of OnShowWithPageVisibility. Disabling will revert the refactor and use the
// direct ShowInternal path.
BASE_DECLARE_FEATURE(kOnShowWithPageVisibility);

// Enables skipping of calls to hideSoftInputFromWindow when there is not a
// keyboard currently visible.
BASE_DECLARE_FEATURE(kOptimizeImmHideCalls);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, queues navigations instead of cancelling the previous
// navigation if the previous navigation is already waiting for commit.
// See https://crbug.com/838348 and https://crbug.com/1220337.
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

// When enabled, CanAccessDataForOrigin can only be called from the UI thread.
// This is related to Citadel desktop protections. See
// https://crbug.com/1286501.
BASE_DECLARE_FEATURE(kRestrictCanAccessDataForOriginToUIThread);

// (crbug/1377753): Speculatively start service worker before BeforeUnload runs.
BASE_DECLARE_FEATURE(kSpeculativeServiceWorkerStartup);

// Please keep features in alphabetical order.

}  // namespace content

#endif  // CONTENT_COMMON_FEATURES_H_
