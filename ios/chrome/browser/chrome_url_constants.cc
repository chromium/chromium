// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/chrome_url_constants.h"

#include <stddef.h>

#include "base/stl_util.h"

const char kChromeUIScheme[] = "chrome";

const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIInspectURL[] = "chrome://inspect/";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUINTPTilesInternalsURL[] = "chrome://ntp-tiles-internals/";
const char kChromeUIOfflineURL[] = "chrome://offline/";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUISuggestionsURL[] = "chrome://suggestions/";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIVersionURL[] = "chrome://version/";

const char kChromeUIAutofillInternalsHost[] = "autofill-internals";
const char kChromeUIBrowserCrashHost[] = "inducebrowsercrashforrealz";
const char kChromeUICrashHost[] = "crash";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUIDinoHost[] = "dino";
const char kChromeUIExternalFileHost[] = "external-file";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIGCMInternalsHost[] = "gcm-internals";
const char kChromeUIHistogramHost[] = "histograms";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIInspectHost[] = "inspect";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUINTPTilesInternalsHost[] = "ntp-tiles-internals";
const char kChromeUIOfflineHost[] = "offline";
const char kChromeUIOmahaHost[] = "omaha";
const char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUIPrefsInternalsHost[] = "prefs-internals";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUISuggestionsHost[] = "suggestions";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUITermsHost[] = "terms";
const char kChromeUITranslateInternalsHost[] = "translate-internals";
const char kChromeUIURLKeyedMetricsHost[] = "ukm";
const char kChromeUIUserActionsHost[] = "user-actions";
const char kChromeUIVersionHost[] = "version";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
// 'histograms' is chrome WebUI on iOS, content WebUI on other platforms.
const char* const kChromeHostURLs[] = {
    kChromeUIChromeURLsHost,
    kChromeUICreditsHost,
    kChromeUIFlagsHost,
    kChromeUIHistogramHost,
    kChromeUIInspectHost,
    kChromeUINetExportHost,
    kChromeUINewTabHost,
    kChromeUINTPTilesInternalsHost,
    kChromeUIPasswordManagerInternalsHost,
    kChromeUISignInInternalsHost,
    kChromeUISuggestionsHost,
    kChromeUISyncInternalsHost,
    kChromeUITermsHost,
    kChromeUIUserActionsHost,
    kChromeUIVersionHost,
};
const size_t kNumberOfChromeHostURLs = base::size(kChromeHostURLs);

const char kSyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync/";

const char kManageYourGoogleAccountURL[] = "https://myaccount.google.com/";

const char kPageInfoHelpCenterURL[] =
    "https://support.google.com/chrome?p=ui_security_indicator&ios=1";

const char kCrashReasonURL[] =
    "https://support.google.com/chrome/answer/95669?p=e_awsnap&ios=1";

const char kPrivacyLearnMoreURL[] =
    "https://support.google.com/chrome/answer/114836?p=settings_privacy&ios=1";

const char kDoNotTrackLearnMoreURL[] =
    "https://support.google.com/chrome/answer/"
    "2942429?p=mobile_do_not_track&ios=1";

const char kSyncEncryptionHelpURL[] =
    "https://support.google.com/chrome/answer/"
    "1181035?p=settings_encryption&ios=1";

const char kClearBrowsingDataLearnMoreURL[] =
    "https://support.google.com/chrome/answer/2392709";

const char kClearBrowsingDataMyActivityUrlInFooterURL[] =
    "https://history.google.com/history/?utm_source=chrome_cbd";

const char kClearBrowsingDataMyActivityUrlInDialogURL[] =
    "https://history.google.com/history/?utm_source=chrome_n";

const char kHistoryMyActivityURL[] =
    "https://history.google.com/history/?utm_source=chrome_h";

const char kGoogleHistoryURL[] = "https://history.google.com";

const char kGoogleMyAccountURL[] =
    "https://myaccount.google.com/privacy#activitycontrols";

const char kReadingListReferrerURL[] =
    "chrome://do_not_consider_for_most_visited/reading_list";

const char kChromeUIAboutNewTabURL[] = "about://newtab/";
