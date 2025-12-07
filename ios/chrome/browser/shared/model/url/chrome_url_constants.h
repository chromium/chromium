// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_URL_CHROME_URL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_URL_CHROME_URL_CONSTANTS_H_

#include <array>
#include <string_view>

#include "components/commerce/core/commerce_constants.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "components/webui/regional_capabilities_internals/constants.h"
#include "ios/components/webui/web_ui_url_constants.h"

// Contains constants for known URLs and portions thereof.

// chrome: URLs (including schemes). Should be kept in sync with the
// URL components below.
inline constexpr char kChromeDinoGameURL[] = "chrome://dino/";
inline constexpr char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
inline constexpr char kChromeUICookiesSettingsURL[] =
    "chrome://settings/coookies";
inline constexpr char kChromeUICreditsURL[] = "chrome://credits/";
inline constexpr char kChromeUIFlagsURL[] = "chrome://flags/";
inline constexpr char kChromeUIHistoryURL[] = "chrome://history/";
inline constexpr char kChromeUIInspectURL[] = "chrome://inspect/";
inline constexpr char kChromeUIInterstitialsURL[] = "chrome://interstitials";
inline constexpr char kChromeUIManagementURL[] = "chrome://management";
inline constexpr char kChromeUINewTabURL[] = "chrome://newtab/";
inline constexpr char kChromeUINTPTilesInternalsURL[] =
    "chrome://ntp-tiles-internals/";
inline constexpr char kChromeUIOfflineURL[] = "chrome://offline/";
inline constexpr char kChromeUIOnDeviceLlmInternalsURL[] =
    "chrome://on-device-llm-internals/";
inline constexpr char kChromeUIPolicyURL[] = "chrome://policy/";
inline constexpr char kChromeUIPolicyLogsURL[] = "chrome://policy/logs";
inline constexpr char kChromeUIPolicyTestURL[] = "chrome://policy/test";
inline constexpr char kChromeUISettingsURL[] = "chrome://settings/";
inline constexpr char kChromeUITermsURL[] = "chrome://terms/";
inline constexpr char kChromeUIVersionURL[] = "chrome://version/";

// URL components for Chrome on iOS.
inline constexpr char kChromeUIAutofillInternalsHost[] = "autofill-internals";
inline constexpr char kChromeUIBrowserCrashHost[] =
    "inducebrowsercrashforrealz";
inline constexpr char kChromeUICrashHost[] = "crash";
inline constexpr char kChromeUIChromeURLsHost[] = "chrome-urls";
inline constexpr char kChromeUICrashesHost[] = "crashes";
inline constexpr char kChromeUICreditsHost[] = "credits";
inline constexpr char kChromeUIDataSharingInternalsHost[] =
    "data-sharing-internals";
inline constexpr char kChromeUIDinoHost[] = "dino";
inline constexpr char kChromeUIDownloadInternalsHost[] = "download-internals";
inline constexpr char kChromeUIExternalFileHost[] = "external-file";
inline constexpr char kChromeUIFlagsHost[] = "flags";
inline constexpr char kChromeUIGCMInternalsHost[] = "gcm-internals";
inline constexpr char kChromeUIHistogramHost[] = "histograms";
inline constexpr char kChromeUIHistoryHost[] = "history";
inline constexpr char kChromeUIInspectHost[] = "inspect";
inline constexpr char kChromeUIInterstitialsHost[] = "interstitials";
inline constexpr char kChromeUILocalStateHost[] = "local-state";
inline constexpr char kChromeUIManagementHost[] = "management";
inline constexpr char kChromeUINetExportHost[] = "net-export";
inline constexpr char kChromeUINewTabHost[] = "newtab";
inline constexpr char kChromeUINTPTilesInternalsHost[] = "ntp-tiles-internals";
inline constexpr char kChromeUIOfflineHost[] = "offline";
inline constexpr char kChromeUIOmahaHost[] = "omaha";
inline constexpr char kChromeUIOnDeviceLlmInternalsHost[] =
    "on-device-llm-internals";
inline constexpr char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
inline constexpr char kChromeUIPolicyHost[] = "policy";
inline constexpr char kChromeUIPrefsInternalsHost[] = "prefs-internals";
inline constexpr char kChromeUIProfileInternalsHost[] = "profile-internals";
inline constexpr char kChromeUISignInInternalsHost[] = "signin-internals";
inline constexpr char kChromeUITermsHost[] = "terms";
inline constexpr char kChromeUITranslateInternalsHost[] = "translate-internals";
inline constexpr char kChromeUIURLKeyedMetricsHost[] = "ukm";
inline constexpr char kChromeUIUserActionsHost[] = "user-actions";
inline constexpr char kChromeUIUserDefaultsInternalsHost[] =
    "userdefaults-internals";
inline constexpr char kChromeUIVersionHost[] = "version";
inline constexpr char kChromeUIDownloadsHost[] = "downloads";

// Legacy URL to the sync google dashboard.
inline constexpr char kLegacySyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync/";

// New URL to the sync google dashboard.
inline constexpr char kNewSyncGoogleDashboardURL[] =
    "https://chrome.google.com/data";

// URL to opt-in to on-device encryption.
inline constexpr char kOnDeviceEncryptionOptInURL[] =
    "https://passwords.google.com/encryption/enroll/intro?"
    "utm_source=chrome&utm_medium=ios&utm_campaign=encryption_enroll";

// URL to learn more about on-device encryption when the user opted-in.
inline constexpr char kOnDeviceEncryptionLearnMoreURL[] =
    "https://support.google.com/accounts?p=settings_password_ode";

// "What do these mean?" URL for the Page Info bubble.
inline constexpr char kPageInfoHelpCenterURL[] =
    "https://support.google.com/chrome?p=ui_security_indicator&ios=1";

// "Learn more" URL for "Aw snap" page when showing "Reload" button.
inline constexpr char kCrashReasonURL[] =
    "https://support.google.com/chrome/answer/95669?p=e_awsnap&ios=1";

// "Learn more" URL for the Privacy section under Options.
inline constexpr char kPrivacyLearnMoreURL[] =
    "https://support.google.com/chrome/answer/114836?p=settings_privacy&ios=1";

// "Terms of service" URL.
inline constexpr char kTermsOfServiceURL[] =
    "https://policies.google.com/terms";

// "Terms of service" URL for mobile view.
inline constexpr char kEmbeddedTermsOfServiceURL[] =
    "https://policies.google.com/terms/embedded";

// The URL for the "Learn more" page on sync encryption.
inline constexpr char kSyncEncryptionHelpURL[] =
    "https://support.google.com/chrome/answer/"
    "1181035?p=settings_encryption&ios=1";

// Google MyActivity URL for the footer in Clear Browsing Data in the
// Privacy section post link update.
inline constexpr char kClearBrowsingDataDSEMyActivityUrlInFooterURL[] =
    "https://myactivity.google.com/myactivity?utm_source=chrome_cbd";

// Google search history URL for the footer in Clear Browsing Data in the
// Privacy section post link update.
inline constexpr char kClearBrowsingDataDSESearchUrlInFooterURL[] =
    "https://myactivity.google.com/product/search?utm_source=chrome_cbd";

// Google history URL for the dialog that informs the user that the history data
// in the Clear Browsing Data under Privacy Options.
inline constexpr char kClearBrowsingDataMyActivityUrlInDialogURL[] =
    "https://history.google.com/history/?utm_source=chrome_n";

// Google history URL for the header notifying the user of other forms of
// browsing history on the history page.
inline constexpr char kHistoryMyActivityURL[] =
    "https://history.google.com/history/?utm_source=chrome_h";

// Google history URL for the Clear Browsing Data under Privacy Options.
// Obsolete: This is no longer used and will removed.
inline constexpr char kGoogleHistoryURL[] = "https://history.google.com";

// Google my account URL for the sign-in confirmation screen.
inline constexpr char kGoogleMyAccountURL[] =
    "https://myaccount.google.com/privacy#activitycontrols";

// URL of the Google Account page showing the known user devices.
inline constexpr char kGoogleMyAccountDeviceActivityURL[] =
    "https://myaccount.google.com/device-activity?utm_source=chrome";

// URL of the Google Account home address page.
inline constexpr char kGoogleMyAccountHomeAddressURL[] =
    "https://myaccount.google.com/address/"
    "home?utm_source=chrome&utm_campaign=manage_addresses";

// URL of the Google Account work address page.
inline constexpr char kGoogleMyAccountWorkAddressURL[] =
    "https://myaccount.google.com/address/"
    "work?utm_source=chrome&utm_campaign=manage_addresses";

// URL of the change Google Account name page.
inline constexpr char kGoogleAccountNameEmailAddressEditURL[] =
    "https://myaccount.google.com/"
    "personal-info?utm_source=chrome-settings&utm_medium=autofill";

// URL used in referrer to signal that the navigation originates from Reading
// List page and thus should not be considered for Most Visited.
inline constexpr char kReadingListReferrerURL[] =
    "chrome://do_not_consider_for_most_visited/reading_list";

// URL used internally by ios/web when loading the NTP.
inline constexpr char kChromeUIAboutNewTabURL[] = "about://newtab/";

// "Learn more" URL for enterprise management information.
inline constexpr char kManagementLearnMoreURL[] =
    "https://support.google.com/chrome/?p=is_chrome_managed";

// "Learn more" URL for the safe browsing setting in the privacy and security
// section.
inline constexpr char kEnhancedSafeBrowsingLearnMoreURL[] =
    "https://support.google.com/chrome?p=safe_browsing_preferences";

// "My Activity" URL for managing the user's activity
inline constexpr char kMyActivityURL[] =
    "https://myactivity.google.com/myactivity";

// "Learn more" URL for the Lens Overlay.
inline constexpr char kLearnMoreLensURL[] =
    "https://support.google.com/chrome/?p=google_lens_ios";

// URL for the BWG App Activity Settings row.
inline constexpr char kBWGAppActivityURL[] =
    "https://myactivity.google.com/product/gemini?utm_source=gemini&pli=1";

// URL for the BWG Precise Location Settings row.
inline constexpr char kBWGPreciseLocationURL[] =
    "http://support.google.com/gemini?p=gcr_location_info";

// URL for the BWG Page Content Sharing Settings row.
inline constexpr char kBWGPageContentSharingURL[] =
    "https://support.google.com/gemini?p=chrome_PH#topic=15280100";

// URL for the BWG Extensions Settings row.
inline constexpr char kBWGExtensionsURL[] = "https://gemini.google.com/apps";

// Gets the hosts/domains that are shown in chrome://chrome-urls.
inline constexpr std::array<std::string_view, 21> kChromeHostURLs = {
    commerce::kChromeUICommerceInternalsHost,
    kChromeUIChromeURLsHost,
    kChromeUICreditsHost,
    kChromeUIDownloadInternalsHost,
    kChromeUIFlagsHost,
    kChromeUIHistogramHost,
    kChromeUIInspectHost,
    kChromeUIInterstitialsHost,
    kChromeUILocalStateHost,
    kChromeUIManagementHost,
    kChromeUINetExportHost,
    kChromeUINewTabHost,
    kChromeUINTPTilesInternalsHost,
    kChromeUIPasswordManagerInternalsHost,
    kChromeUISignInInternalsHost,
    kChromeUISyncInternalsHost,
    kChromeUITermsHost,
    kChromeUIUserActionsHost,
    kChromeUIVersionHost,
    optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost,
    regional_capabilities::kChromeUIRegionalCapabilitiesInternalsHost,
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_URL_CHROME_URL_CONSTANTS_H_
