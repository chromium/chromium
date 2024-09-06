// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_URL_CHROME_URL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_URL_CHROME_URL_CONSTANTS_H_

#include <stddef.h>

// Contains constants for known URLs and portions thereof.

// chrome: URLs (including schemes). Should be kept in sync with the
// URL components below.
extern const char kChromeDinoGameURL[];
extern const char kChromeUIChromeURLsURL[];
extern const char kChromeUICookiesSettingsURL[];
extern const char kChromeUICreditsURL[];
extern const char kChromeUIFlagsURL[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIInspectURL[];
extern const char kChromeUIIntersitialsURL[];
extern const char kChromeUIManagementURL[];
extern const char kChromeUIOnDeviceLlmInternalsURL[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUINTPTilesInternalsURL[];
extern const char kChromeUIOfflineURL[];
extern const char kChromeUIPolicyURL[];
extern const char kChromeUIPopularSitesInternalsURL[];
extern const char kChromeUISettingsURL[];
extern const char kChromeUITermsURL[];
extern const char kChromeUIVersionURL[];

// URL components for Chrome on iOS.
extern const char kChromeUIAutofillInternalsHost[];
extern const char kChromeUIBrowserCrashHost[];
extern const char kChromeUIChromeURLsHost[];
extern const char kChromeUICrashesHost[];
extern const char kChromeUICrashHost[];
extern const char kChromeUICreditsHost[];
extern const char kChromeUIDinoHost[];
extern const char kChromeUIDownloadInternalsHost[];
extern const char kChromeUIExternalFileHost[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIGCMInternalsHost[];
extern const char kChromeUIHistogramHost[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIInspectHost[];
extern const char kChromeUIIntersitialsHost[];
extern const char kChromeUILocalStateHost[];
extern const char kChromeUIManagementHost[];
extern const char kChromeUINetExportHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUINTPTilesInternalsHost[];
extern const char kChromeUIOfflineHost[];
extern const char kChromeUIOmahaHost[];
extern const char kChromeUIOnDeviceLlmInternalsHost[];
extern const char kChromeUIPasswordManagerInternalsHost[];
extern const char kChromeUIPolicyHost[];
extern const char kChromeUIPopularSitesInternalsHost[];
extern const char kChromeUIPrefsInternalsHost[];
extern const char kChromeUISignInInternalsHost[];
extern const char kChromeUITermsHost[];
extern const char kChromeUITranslateInternalsHost[];
extern const char kChromeUIURLKeyedMetricsHost[];
extern const char kChromeUIUserActionsHost[];
extern const char kChromeUIUserDefaultsInternalsHost[];
extern const char kChromeUIVersionHost[];
extern const char kChromeUIDownloadsHost[];

// Gets the hosts/domains that are shown in chrome://chrome-urls.
extern const char* const kChromeHostURLs[];
extern const size_t kNumberOfChromeHostURLs;

// URL to the sync google dashboard.
extern const char kSyncGoogleDashboardURL[];

// URL to opt-in to on-device encryption.
extern const char kOnDeviceEncryptionOptInURL[];

// URL to learn more about on-device encryption when the user opted-in.
extern const char kOnDeviceEncryptionLearnMoreURL[];

// "What do these mean?" URL for the Page Info bubble.
extern const char kPageInfoHelpCenterURL[];

// "Learn more" URL for "Aw snap" page when showing "Reload" button.
extern const char kCrashReasonURL[];

// "Learn more" URL for the Privacy section under Options.
extern const char kPrivacyLearnMoreURL[];

// "Terms of service" URL.
extern const char kTermsOfServiceURL[];

// "Terms of service" URL for mobile view.
extern const char kEmbeddedTermsOfServiceURL[];

// The URL for the "Learn more" page on sync encryption.
extern const char kSyncEncryptionHelpURL[];

// Google history URL for the footer in the Clear Browsing Data under Privacy
// Options.
extern const char kClearBrowsingDataMyActivityUrlInFooterURL[];

// Google MyActivity URL for the footer in Clear Browsing Data in the
// Privacy section post link update.
extern const char kClearBrowsingDataDSEMyActivityUrlInFooterURL[];

// Google search history URL for the footer in Clear Browsing Data in the
// Privacy section post link update.
extern const char kClearBrowsingDataDSESearchUrlInFooterURL[];

// Google history URL for the dialog that informs the user that the history data
// in the Clear Browsing Data under Privacy Options.
extern const char kClearBrowsingDataMyActivityUrlInDialogURL[];

// Google history URL for the header notifying the user of other forms of
// browsing history on the history page.
extern const char kHistoryMyActivityURL[];

// Google history URL for the Clear Browsing Data under Privacy Options.
// Obsolete: This is no longer used and will removed.
extern const char kGoogleHistoryURL[];

// Google my account URL for the sign-in confirmation screen.
extern const char kGoogleMyAccountURL[];

// URL of the Google Account page showing the known user devices.
extern const char kGoogleMyAccountDeviceActivityURL[];

// URL used in referrer to signal that the navigation originates from Reading
// List page and thus should not be considered for Most Visited.
extern const char kReadingListReferrerURL[];

// URL used internally by ios/web when loading the NTP.
extern const char kChromeUIAboutNewTabURL[];

// "Learn more" URL for enterprise management information.
extern const char kManagementLearnMoreURL[];

// "Learn more" URL for the safe browsing setting in the privacy and security
// section.
extern const char kEnhancedSafeBrowsingLearnMoreURL[];

// "My Activity" URL for managing the user's activity
extern const char kMyActivityURL[];

// "Learn more" URL for the Lens Overlay.
extern const char kLearnMoreLensURL[];

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_URL_CHROME_URL_CONSTANTS_H_
