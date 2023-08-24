// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace content {

// Please keep features in alphabetical order.

BASE_FEATURE(kBeforeUnloadBrowserResponseQueue,
             "BeforeUnloadBrowserResponseQueue",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOptimizeImmHideCalls,
             "OptimizeImmHideCalls",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kConsolidatedIPCForProxyCreation,
             "ConsolidatedIPCForProxyCreation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnsureAllowBindingsIsAlwaysForWebUI,
             "EnsureAllowBindingsIsAlwaysForWebUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kGpuInfoCollectionSeparatePrefetch,
             "GpuInfoCollectionSeparatePrefetch",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kRestrictCanAccessDataForOriginToUIThread,
             "RestrictCanAccessDataForOriginToUIThread",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kServiceWorkerAutoPreload,
             "ServiceWorkerAutoPreload",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kServiceWorkerStaticRouterStartServiceWorker,
             "ServiceWorkerStaticRouterStartServiceWorker",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSiteIsolationCitadelEnforcement,
             "kSiteIsolationCitadelEnforcement",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSpeculativeServiceWorkerStartup,
             "SpeculativeServiceWorkerStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWindowOpenFileSelectFix,
             "WindowOpenFileSelectFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Please keep features in alphabetical order.

}  // namespace content
