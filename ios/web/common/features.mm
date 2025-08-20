// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/features.h"

#import "base/metrics/field_trial_params.h"
#import "build/blink_buildflags.h"

namespace web {
namespace features {

BASE_FEATURE(CrashOnUnexpectedURLChange, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBlockUniversalLinksInOffTheRecordMode,
             "BlockUniversalLinksInOffTheRecord",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(KeepsRenderProcessAlive, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ClearOldNavigationRecordsWorkaround,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(EnablePersistentDownloads, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(SetRequestAttribution, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(IOSSharedHighlightingColorChange,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableMeasurements,
             "EnableMeasurementsExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ScrollViewProxyScrollEnabledWorkaround,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PreventNavigationWithoutUserInteraction,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowCrossWindowExternalAppNavigation,
             "kAllowCrossWindowExternalAppNavigation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(EnableWebInspector, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSmoothScrollingDefault,
             "FullscreenSmoothScrollingDefault",
#if BUILDFLAG(USE_BLINK)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(FullscreenScrollThreshold, base::FEATURE_DISABLED_BY_DEFAULT);

const char kFullscreenScrollThresholdAmount[] =
    "fullscreen_scroll_threshold_amount";

bool IsFullscreenScrollThresholdEnabled() {
  return !base::FeatureList::IsEnabled(kSmoothScrollingDefault) &&
         base::FeatureList::IsEnabled(kFullscreenScrollThreshold);
}

// This feature will always be disabled and will only be enabled by tests.
BASE_FEATURE(ForceSynthesizedRestoreSession, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(DetectDestroyedNavigationContexts,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsWebInspectorSupportEnabled() {
  if (@available(iOS 16.4, *)) {
    return base::FeatureList::IsEnabled(kEnableWebInspector);
  }
  return false;
}

BASE_FEATURE(DisableRaccoon, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(UserAgentBugFixVersion, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(LogJavaScriptErrors, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(WebKitHandlesMarketplaceKitLinks,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(RestoreWKWebViewEditMenuHandler,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LogCrWebJavaScriptErrors, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(AssertOnJavaScriptErrors, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace web
