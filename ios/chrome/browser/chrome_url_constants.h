// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_URL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CHROME_URL_CONSTANTS_H_

#include <stddef.h>

// Contains constants for known URLs and portions thereof.

// TODO(blundell): This file should be ios_chrome_url_constants.*, and all of
// these constants should have a kIOSChrome prefix instead of a kChrome
// prefix. crbug.com/537174

// URL scheme for Chrome on iOS. This needs to be kept in sync with the constant
// kChromeUIScheme defined in content/public/common/url_constants.h until Chrome
// on iOS stops depending on //content downstream.
extern const char kChromeUIScheme[];

// chrome: URLs (including schemes). Should be kept in sync with the
// URL components below.
extern const char kChromeUIChromeURLsURL[];
extern const char kChromeUICreditsURL[];
extern const char kChromeUIFlagsURL[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIInspectURL[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUINTPTilesInternalsURL[];
extern const char kChromeUIOfflineURL[];
extern const char kChromeUIPopularSitesInternalsURL[];
extern const char kChromeUISettingsURL[];
extern const char kChromeUISuggestionsURL[];
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
extern const char kChromeUIExternalFileHost[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIGCMInternalsHost[];
extern const char kChromeUIHistogramHost[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIInspectHost[];
extern const char kChromeUINetExportHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUINTPTilesInternalsHost[];
extern const char kChromeUIOfflineHost[];
extern const char kChromeUIOmahaHost[];
extern const char kChromeUIPasswordManagerInternalsHost[];
extern const char kChromeUIPolicyHost[];
extern const char kChromeUIPopularSitesInternalsHost[];
extern const char kChromeUIPrefsInternalsHost[];
extern const char kChromeUISignInInternalsHost[];
extern const char kChromeUISuggestionsHost[];
extern const char kChromeUISyncInternalsHost[];
extern const char kChromeUITermsHost[];
extern const char kChromeUITranslateInternalsHost[];
extern const char kChromeUIURLKeyedMetricsHost[];
extern const char kChromeUIUserActionsHost[];
extern const char kChromeUIVersionHost[];

// Gets the hosts/domains that are shown in chrome://chrome-urls.
extern const char* const kChromeHostURLs[];
extern const size_t kNumberOfChromeHostURLs;

// URL to the sync google dashboard.
extern const char kSyncGoogleDashboardURL[];

// URL to the Google account configuration page.
extern const char kManageYourGoogleAccountURL[];

// "What do these mean?" URL for the Page Info bubble.
extern const char kPageInfoHelpCenterURL[];

// "Learn more" URL for "Aw snap" page when showing "Reload" button.
extern const char kCrashReasonURL[];

// "Learn more" URL for the Privacy section under Options.
extern const char kPrivacyLearnMoreURL[];

// "Learn more" URL for the "Do not track" setting in the privacy section.
extern const char kDoNotTrackLearnMoreURL[];

// The URL for the "Learn more" page on sync encryption.
extern const char kSyncEncryptionHelpURL[];

// "Learn more" URL for the Clear Browsing Data under Privacy Options.
extern const char kClearBrowsingDataLearnMoreURL[];

// Google history URL for the footer in the Clear Browsing Data under Privacy
// Options.
extern const char kClearBrowsingDataMyActivityUrlInFooterURL[];

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

// URL used in referrer to signal that the navigation originates from Reading
// List page and thus should not be considered for Most Visited.
extern const char kReadingListReferrerURL[];

// URL used internally by ios/web when loading the NTP.
extern const char kChromeUIAboutNewTabURL[];

#endif  // IOS_CHROME_BROWSER_CHROME_URL_CONSTANTS_H_
