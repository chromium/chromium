// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/features.h"

namespace web {

BASE_FEATURE(kEnableBrowserLockdownMode,
             "EnableBrowserLockdownMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBrowserLockdownModeEnabled() {
  return base::FeatureList::IsEnabled(kEnableBrowserLockdownMode);
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

}  // namespace web
