// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"

#include <stddef.h>

#include <iterator>

#include "components/commerce/core/commerce_constants.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "ios/components/webui/web_ui_url_constants.h"

const char kChromeDinoGameURL[] = "chrome://dino/";
const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUICookiesSettingsURL[] = "chrome://settings/coookies";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIInspectURL[] = "chrome://inspect/";
const char kChromeUIIntersitialsURL[] = "chrome://interstitials";
const char kChromeUIManagementURL[] = "chrome://management";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUINTPTilesInternalsURL[] = "chrome://ntp-tiles-internals/";
const char kChromeUIOfflineURL[] = "chrome://offline/";
const char kChromeUIOnDeviceLlmInternalsURL[] =
    "chrome://on-device-llm-internals/";
const char kChromeUIPolicyURL[] = "chrome://policy/";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIVersionURL[] = "chrome://version/";

const char kChromeUIAutofillInternalsHost[] = "autofill-internals";
const char kChromeUIBrowserCrashHost[] = "inducebrowsercrashforrealz";
const char kChromeUICrashHost[] = "crash";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUIDinoHost[] = "dino";
const char kChromeUIDownloadInternalsHost[] = "download-internals";
const char kChromeUIExternalFileHost[] = "external-file";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIGCMInternalsHost[] = "gcm-internals";
const char kChromeUIHistogramHost[] = "histograms";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIInspectHost[] = "inspect";
const char kChromeUIIntersitialsHost[] = "interstitials";
const char kChromeUILocalStateHost[] = "local-state";
const char kChromeUIManagementHost[] = "management";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUINTPTilesInternalsHost[] = "ntp-tiles-internals";
const char kChromeUIOfflineHost[] = "offline";
const char kChromeUIOmahaHost[] = "omaha";
const char kChromeUIOnDeviceLlmInternalsHost[] = "on-device-llm-internals";
const char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUIPrefsInternalsHost[] = "prefs-internals";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUITermsHost[] = "terms";
const char kChromeUITranslateInternalsHost[] = "translate-internals";
const char kChromeUIURLKeyedMetricsHost[] = "ukm";
const char kChromeUIUserActionsHost[] = "user-actions";
const char kChromeUIUserDefaultsInternalsHost[] = "userdefaults-internals";
const char kChromeUIVersionHost[] = "version";
const char kChromeUIDownloadsHost[] = "downloads";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
// 'histograms' is chrome WebUI on iOS, content WebUI on other platforms.
const char* const kChromeHostURLs[] = {
    commerce::kChromeUICommerceInternalsHost,
    kChromeUIChromeURLsHost,
    kChromeUICreditsHost,
    kChromeUIFlagsHost,
    kChromeUIHistogramHost,
    kChromeUIInspectHost,
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
};
const size_t kNumberOfChromeHostURLs = std::size(kChromeHostURLs);

const char kSyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync/";

const char kOnDeviceEncryptionOptInURL[] =
    "https://passwords.google.com/encryption/enroll/intro?"
    "utm_source=chrome&utm_medium=ios&utm_campaign=encryption_enroll";

const char kOnDeviceEncryptionLearnMoreURL[] =
    "https://support.google.com/accounts?p=settings_password_ode";

const char kPageInfoHelpCenterURL[] =
    "https://support.google.com/chrome?p=ui_security_indicator&ios=1";

const char kCrashReasonURL[] =
    "https://support.google.com/chrome/answer/95669?p=e_awsnap&ios=1";

const char kPrivacyLearnMoreURL[] =
    "https://support.google.com/chrome/answer/114836?p=settings_privacy&ios=1";

const char kTermsOfServiceURL[] = "https://policies.google.com/terms";

const char kEmbeddedTermsOfServiceURL[] =
    "https://policies.google.com/terms/embedded";

const char kSyncEncryptionHelpURL[] =
    "https://support.google.com/chrome/answer/"
    "1181035?p=settings_encryption&ios=1";

const char kClearBrowsingDataMyActivityUrlInFooterURL[] =
    "https://history.google.com/history/?utm_source=chrome_cbd";

const char kClearBrowsingDataDSEMyActivityUrlInFooterURL[] =
    "https://myactivity.google.com/myactivity?utm_source=chrome_cbd";

const char kClearBrowsingDataDSESearchUrlInFooterURL[] =
    "https://myactivity.google.com/product/search?utm_source=chrome_cbd";

const char kClearBrowsingDataMyActivityUrlInDialogURL[] =
    "https://history.google.com/history/?utm_source=chrome_n";

const char kHistoryMyActivityURL[] =
    "https://history.google.com/history/?utm_source=chrome_h";

const char kGoogleHistoryURL[] = "https://history.google.com";

const char kGoogleMyAccountURL[] =
    "https://myaccount.google.com/privacy#activitycontrols";

const char kGoogleMyAccountDeviceActivityURL[] =
    "https://myaccount.google.com/device-activity?utm_source=chrome";

const char kReadingListReferrerURL[] =
    "chrome://do_not_consider_for_most_visited/reading_list";

const char kChromeUIAboutNewTabURL[] = "about://newtab/";

const char kManagementLearnMoreURL[] =
    "https://support.google.com/chrome/?p=is_chrome_managed";

const char kEnhancedSafeBrowsingLearnMoreURL[] =
    "https://support.google.com/chrome?p=safe_browsing_preferences";

const char kMyActivityURL[] = "https://myactivity.google.com/myactivity";

const char kLearnMoreLensURL[] =
    "https://support.google.com/chrome/?p=google_lens_ios";
