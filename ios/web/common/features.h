// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_FEATURES_H_
#define IOS_WEB_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace web::features {

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

// Feature flag to enable Measurements detection.
BASE_DECLARE_FEATURE(kEnableMeasurements);

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

// Feature flag to enable a scroll threshold before entering or exiting
// fullscreen.
BASE_DECLARE_FEATURE(kFullscreenScrollThreshold);

// Feature flag that force the use of the synthesized native WKWebView
// session instead of the (maybe inexistent) saved native session. The
// purpose of this flag it to allow to testing this code path.
BASE_DECLARE_FEATURE(kForceSynthesizedRestoreSession);

// Feature flag to enable detecting destroyed NavigationContexts. This is
// intended to be used as a kill switch.
BASE_DECLARE_FEATURE(kDetectDestroyedNavigationContexts);

// Feature flag to disable the raccoon.
BASE_DECLARE_FEATURE(kDisableRaccoon);

// Feature flag adds bugfix numbers to the iOS User-Agent header for Chrome
BASE_DECLARE_FEATURE(kUserAgentBugFixVersion);

// Enables logging JavaScript errors.
BASE_DECLARE_FEATURE(kLogJavaScriptErrors);

// Feature flag to let WebKit handle MarketplaceKit links. This is intended to
// be used as a kill switch.
BASE_DECLARE_FEATURE(kWebKitHandlesMarketplaceKitLinks);

// Feature flag to restore the WKWebView edit menu customization.
BASE_DECLARE_FEATURE(kRestoreWKWebViewEditMenuHandler);

// Enables logging CrWeb Javascript errors.
BASE_DECLARE_FEATURE(kLogCrWebJavaScriptErrors);

// When enabled, JavaScript errors will crash the application.
BASE_DECLARE_FEATURE(kAssertOnJavaScriptErrors);

// Feature controlling when to create TabHelpers.
BASE_DECLARE_FEATURE(kCreateTabHelperOnlyForRealizedWebStates);

// A flag parameter to set the number of pixels to use as the threshold.
inline constexpr char kFullscreenScrollThresholdAmount[] =
    "fullscreen_scroll_threshold_amount";

// Returns true if SmoothScrollingDefault is disabled and
// FullscreenScrollThreshold is enabled.
bool IsFullscreenScrollThresholdEnabled();

// When true, an option to enable Web Inspector should be present in Settings.
bool IsWebInspectorSupportEnabled();

// Returns whether the TabHelpers should only be created for realized WebStates.
bool CreateTabHelperOnlyForRealizedWebStates();

// TODO(crbug.com/449156290): Clean up the kill switch for updating SSL status
// on navigation item creation.
// When enabled, trigger an update of the SSL status on navigation item
// lazy creation. This is intended to be used as a kill switch.
BASE_DECLARE_FEATURE(kUpdateSSLStatusOnNavigationItemLazyCreation);

}  // namespace web::features

#endif  // IOS_WEB_COMMON_FEATURES_H_
