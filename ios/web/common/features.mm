// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/features.h"

#import "base/metrics/field_trial_params.h"
#import "build/blink_buildflags.h"

namespace web {
namespace features {

BASE_FEATURE(kCrashOnUnexpectedURLChange,
             "CrashOnUnexpectedURLChange",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBlockUniversalLinksInOffTheRecordMode,
             "BlockUniversalLinksInOffTheRecord",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kKeepsRenderProcessAlive,
             "KeepsRenderProcessAlive",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClearOldNavigationRecordsWorkaround,
             "ClearOldNavigationRecordsWorkaround",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePersistentDownloads,
             "EnablePersistentDownloads",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSetRequestAttribution,
             "SetRequestAttribution",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSSharedHighlightingColorChange,
             "IOSSharedHighlightingColorChange",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableMeasurements,
             "EnableMeasurementsExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kOneTapForMapsConsentModeParamTitle[] =
    "OneTapForMapsConsentModeParam";
const char kOneTapForMapsConsentModeDefaultParam[] = "default";
const char kOneTapForMapsConsentModeForcedParam[] = "forced";
const char kOneTapForMapsConsentModeDisabledParam[] = "disabled";
const char kOneTapForMapsConsentModeIPHParam[] = "iph";
const char kOneTapForMapsConsentModeIPHForcedParam[] = "iphforced";
BASE_FEATURE(kOneTapForMaps,
             "EnableOneTapForMaps",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kScrollViewProxyScrollEnabledWorkaround,
             "ScrollViewProxyScrollEnabledWorkaround",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPreventNavigationWithoutUserInteraction,
             "PreventNavigationWithoutUserInteraction",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowCrossWindowExternalAppNavigation,
             "kAllowCrossWindowExternalAppNavigation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebInspector,
             "EnableWebInspector",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSmoothScrollingDefault,
             "FullscreenSmoothScrollingDefault",
#if BUILDFLAG(USE_BLINK)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// This feature will always be disabled and will only be enabled by tests.
BASE_FEATURE(kForceSynthesizedRestoreSession,
             "ForceSynthesizedRestoreSession",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDetectDestroyedNavigationContexts,
             "DetectDestroyedNavigationContexts",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableViewportIntents,
             "EnableViewportIntents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableNewParcelTrackingNumberDetection,
             "EnableNewParcelTrackingNumberDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsWebInspectorSupportEnabled() {
  if (@available(iOS 16.4, *)) {
    return base::FeatureList::IsEnabled(kEnableWebInspector);
  }
  return false;
}

BASE_FEATURE(kDisableRaccoon,
             "DisableRaccoon",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUserAgentBugFixVersion,
             "UserAgentBugFixVersion",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLogJavaScriptErrors,
             "LogJavaScriptErrors",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace web
