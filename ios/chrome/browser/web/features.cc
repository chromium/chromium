// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/features.h"

#include "ios/web/common/features.h"

namespace web {

BASE_FEATURE(kBrowserLockdownModeAvailable,
             "BrowserLockdownModeAvailable",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBrowserLockdownModeEnabled() {
  return base::FeatureList::IsEnabled(kBrowserLockdownModeAvailable);
}

BASE_FEATURE(kWebPageDefaultZoomFromDynamicType,
             "WebPageDefaultZoomFromDynamicType",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebPageAlternativeTextZoom,
             "WebPageAlternativeTextZoom",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebPageTextZoomIPad,
             "WebPageTextZoomIPad",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRestoreSessionFromCache,
             "RestoreSessionFromCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool UseNativeSessionRestorationCache() {
  // The optimised session restoration code manage the session state save
  // itself, so there is no need to use the native session restoration cache
  // when the feature is enabled.
  if (web::features::UseSessionSerializationOptimizations()) {
    return false;
  }

  return base::FeatureList::IsEnabled(web::kRestoreSessionFromCache);
}

}  // namespace web
