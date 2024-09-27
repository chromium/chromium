// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_FEATURES_H_
#define IOS_WEB_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace web {
namespace features {

// Used to crash the browser if unexpected URL change is detected.
// https://crbug.com/841105.
BASE_DECLARE_FEATURE(kCrashOnUnexpectedURLChange);

// Used to prevent native apps from being opened when a universal link is tapped
// and the user is browsing in off the record mode.
BASE_DECLARE_FEATURE(kBlockUniversalLinksInOffTheRecordMode);

// Used to ensure that the render is not suspended.
BASE_DECLARE_FEATURE(kKeepsRenderProcessAlive);

// Used to enable the workaround for a WKWebView WKNavigation leak.
// (crbug.com/1010765).  Clear older pending navigation records when a
// navigation finishes.
BASE_DECLARE_FEATURE(kClearOldNavigationRecordsWorkaround);

// Feature flag enabling persistent downloads.
BASE_DECLARE_FEATURE(kEnablePersistentDownloads);

// When enabled, the `attribution` property of NSMutableURLRequests passed to
// WKWebView is set as NSURLRequestAttributionUser on iOS 15.
BASE_DECLARE_FEATURE(kSetRequestAttribution);

// Feature flag that enable Shared Highlighting color change in iOS.
BASE_DECLARE_FEATURE(kIOSSharedHighlightingColorChange);

// Feature flag to enable Measurements detection.
BASE_DECLARE_FEATURE(kEnableMeasurements);

// Feature param under kOneTapForMaps to select consent behavior.
extern const char kOneTapForMapsConsentModeParamTitle[];
extern const char kOneTapForMapsConsentModeDefaultParam[];
extern const char kOneTapForMapsConsentModeForcedParam[];
extern const char kOneTapForMapsConsentModeDisabledParam[];
extern const char kOneTapForMapsConsentModeIPHParam[];
extern const char kOneTapForMapsConsentModeIPHForcedParam[];
// Feature flag to enable One tap experience for Maps.
BASE_DECLARE_FEATURE(kOneTapForMaps);

// When enabled, CRWWebViewScrollViewProxy's `scrollEnabled` state is not
// restored if the new instance already has the same `scrollEnabled` state as
// the old one.
BASE_DECLARE_FEATURE(kScrollViewProxyScrollEnabledWorkaround);

// Feature flag to prevent navigation without user interaction.
BASE_DECLARE_FEATURE(kPreventNavigationWithoutUserInteraction);

// Feature flag to allow a window to open an external app from another window.
// This flag can be used to kill the cross window limitation in case it breaks a
// legitimate use case.
BASE_DECLARE_FEATURE(kAllowCrossWindowExternalAppNavigation);

// Feature flag to enable Web Inspector support.
BASE_DECLARE_FEATURE(kEnableWebInspector);

// Feature used by finch config to enable smooth scrolling when the default
// viewport adjustment experiment is selected via command line switches.
BASE_DECLARE_FEATURE(kSmoothScrollingDefault);

// Feature flag that force the use of the synthesized native WKWebView
// session instead of the (maybe inexistent) saved native session. The
// purpose of this flag it to allow to testing this code path.
BASE_DECLARE_FEATURE(kForceSynthesizedRestoreSession);

// Feature flag to enable detecting destroyed NavigationContexts. This is
// intended to be used as a kill switch.
BASE_DECLARE_FEATURE(kDetectDestroyedNavigationContexts);

// Feature flag to enable intent detection in viewport only.
BASE_DECLARE_FEATURE(kEnableViewportIntents);

// Feature flag to enable improve tracking number detection.
BASE_DECLARE_FEATURE(kEnableNewParcelTrackingNumberDetection);

// When true, an option to enable Web Inspector should be present in Settings.
bool IsWebInspectorSupportEnabled();

// Feature flag to disable the raccoon.
BASE_DECLARE_FEATURE(kDisableRaccoon);

// Feature flag adds bugfix numbers to the iOS User-Agent header for Chrome
BASE_DECLARE_FEATURE(kUserAgentBugFixVersion);

// Enables logging JavaScript errors.
BASE_DECLARE_FEATURE(kLogJavaScriptErrors);

}  // namespace features
}  // namespace web

#endif  // IOS_WEB_COMMON_FEATURES_H_
