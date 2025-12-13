// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/features.h"

#import "base/metrics/field_trial_params.h"
#import "build/blink_buildflags.h"

namespace web::features {

BASE_FEATURE(kCrashOnUnexpectedURLChange, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBlockUniversalLinksInOffTheRecordMode,
             "BlockUniversalLinksInOffTheRecord",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kKeepsRenderProcessAlive, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClearOldNavigationRecordsWorkaround,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePersistentDownloads, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSetRequestAttribution, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableMeasurements,
             "EnableMeasurementsExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kScrollViewProxyScrollEnabledWorkaround,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPreventNavigationWithoutUserInteraction,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowCrossWindowExternalAppNavigation,
             "kAllowCrossWindowExternalAppNavigation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebInspector, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSmoothScrollingDefault,
             "FullscreenSmoothScrollingDefault",
#if BUILDFLAG(USE_BLINK)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kFullscreenScrollThreshold, base::FEATURE_DISABLED_BY_DEFAULT);

// This feature will always be disabled and will only be enabled by tests.
BASE_FEATURE(kForceSynthesizedRestoreSession,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDetectDestroyedNavigationContexts,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDisableRaccoon, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUserAgentBugFixVersion, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLogJavaScriptErrors, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebKitHandlesMarketplaceKitLinks,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRestoreWKWebViewEditMenuHandler,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLogCrWebJavaScriptErrors, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAssertOnJavaScriptErrors, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCreateTabHelperOnlyForRealizedWebStates,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsFullscreenScrollThresholdEnabled() {
  return !base::FeatureList::IsEnabled(kSmoothScrollingDefault) &&
         base::FeatureList::IsEnabled(kFullscreenScrollThreshold);
}

bool IsWebInspectorSupportEnabled() {
  if (@available(iOS 16.4, *)) {
    return base::FeatureList::IsEnabled(kEnableWebInspector);
  }
  return false;
}

bool CreateTabHelperOnlyForRealizedWebStates() {
  return base::FeatureList::IsEnabled(kCreateTabHelperOnlyForRealizedWebStates);
}

BASE_FEATURE(kUpdateSSLStatusOnNavigationItemLazyCreation,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace web::features
