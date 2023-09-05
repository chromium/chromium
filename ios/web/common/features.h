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

// Records snapshot size of image (IOS.Snapshots.ImageSize histogram) and PDF
// (IOS.Snapshots.PDFSize histogram) if enabled. Enabling this flag will
// generate PDF when Page Snapshot is taken just to record PDF size.
BASE_DECLARE_FEATURE(kRecordSnapshotSize);

// When enabled, the `attribution` property of NSMutableURLRequests passed to
// WKWebView is set as NSURLRequestAttributionUser on iOS 15.
BASE_DECLARE_FEATURE(kSetRequestAttribution);

// Feature flag that enable Shared Highlighting color change in iOS.
BASE_DECLARE_FEATURE(kIOSSharedHighlightingColorChange);

// Feature flag that enables native session restoration with a synthesized
// interaction state.
BASE_DECLARE_FEATURE(kSynthesizedRestoreSession);

// Feature flag enabling use of new iOS 15
// loadSimulatedRequest:responseHTMLString: API to display error pages in
// CRWWKNavigationHandler. The helper method IsLoadSimulatedRequestAPIEnabled()
// should be used instead of directly checking this feature.
BASE_DECLARE_FEATURE(kUseLoadSimulatedRequestForOfflinePage);

// Feature flag to enable Emails detection.
BASE_DECLARE_FEATURE(kEnableEmails);

// Feature flag to enable Phone Numbers detection.
BASE_DECLARE_FEATURE(kEnablePhoneNumbers);

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

// Feature flag that enables using web::AnnotationsTextManager for fetching web
// page text for language detection.
BASE_DECLARE_FEATURE(kUseAnnotationsForLanguageDetection);

// When enabled, CRWWebViewScrollViewProxy's `scrollEnabled` state is not
// restored if the new instance already has the same `scrollEnabled` state as
// the old one.
BASE_DECLARE_FEATURE(kScrollViewProxyScrollEnabledWorkaround);

// Feature flag to prevent navigation without user interaction.
BASE_DECLARE_FEATURE(kPreventNavigationWithoutUserInteraction);

// Feature flag to enable Web Inspector support.
BASE_DECLARE_FEATURE(kEnableWebInspector);

// Feature used by finch config to enable smooth scrolling when the default
// viewport adjustment experiment is selected via command line switches.
BASE_DECLARE_FEATURE(kSmoothScrollingDefault);

// Feature flag to enable the session serialization optimizations.
BASE_DECLARE_FEATURE(kEnableSessionSerializationOptimizations);

// When true, the new loadSimulatedRequest API should be used when displaying
// error pages.
bool IsLoadSimulatedRequestAPIEnabled();

// When true, an option to enable Web Inspector should be present in Settings.
bool IsWebInspectorSupportEnabled();

// When true, session serialization optimizations should be enabled.
bool UseSessionSerializationOptimizations();

}  // namespace features
}  // namespace web

#endif  // IOS_WEB_COMMON_FEATURES_H_
