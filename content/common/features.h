// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FEATURES_H_
#define CONTENT_COMMON_FEATURES_H_

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// Please keep features in alphabetical order.

// When enabled, the browser will schedule before unload tasks that continue
// navigation network responses in a kHigh priority queue.
BASE_DECLARE_FEATURE(kBeforeUnloadBrowserResponseQueue);

// When enabled, RenderFrameHostManager::CommitPending will also update the
// visibility of all child views, not just that of the main frame.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kNavigationUpdatesChildViewsVisibility);

#if BUILDFLAG(IS_ANDROID)
// Enables skipping of calls to hideSoftInputFromWindow when there is not a
// keyboard currently visible.
BASE_DECLARE_FEATURE(kOptimizeImmHideCalls);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, when creating new proxies for all nodes in a `FrameTree`, one
// IPC is sent to create all child frame proxies instead of sending one IPC per
// proxy.
BASE_DECLARE_FEATURE(kConsolidatedIPCForProxyCreation);
// TODO(https://crbug.com/1442346): Feature flag to guard extra CHECKs put in
// place to ensure that the AllowBindings API on RenderFrameHost is not called
// for documents outside of WebUI ones.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kEnsureAllowBindingsIsAlwaysForWebUI);

// Adds "/prefetch:8" (which is the "other" category of process - i.e. not
// browser, gpu, crashpad, etc.) to the info collection GPU process' command
// line, in order to keep from polluting the GPU prefetch history.
#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kGpuInfoCollectionSeparatePrefetch);
#endif

// When enabled, CanAccessDataForOrigin can only be called from the UI thread.
// This is related to Citadel desktop protections. See
// https://crbug.com/1286501.
BASE_DECLARE_FEATURE(kRestrictCanAccessDataForOriginToUIThread);

// (crbug.com/1371756): When enabled, the static routing API starts
// ServiceWorker when the routing result of a main resource request was network
// fallback.
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kServiceWorkerStaticRouterStartServiceWorker);

// When enabled, ensures that an unlocked process cannot access data for
// sites that require a dedicated process.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSiteIsolationCitadelEnforcement);

// (crbug/1377753): Speculatively start service worker before BeforeUnload runs.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSpeculativeServiceWorkerStartup);

// Please keep features in alphabetical order.

}  // namespace content

#endif  // CONTENT_COMMON_FEATURES_H_
