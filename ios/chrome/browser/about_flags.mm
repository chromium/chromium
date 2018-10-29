// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of about_flags for iOS that sets flags based on experimental
// settings.

#include "ios/chrome/browser/about_flags.h"

#include <stddef.h>
#include <stdint.h>
#import <UIKit/UIKit.h>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_info.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/ntp_tiles/switches.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#include "components/search_provider_logos/switches.h"
#include "components/security_state/core/features.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/app_launcher/app_launcher_flags.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/drag_and_drop/drag_and_drop_flag.h"
#include "ios/chrome/browser/ios_chrome_flag_descriptions.h"
#include "ios/chrome/browser/itunes_urls/itunes_urls_flag.h"
#include "ios/chrome/browser/mailto/features.h"
#include "ios/chrome/browser/search_engines/feature_flags.h"
#include "ios/chrome/browser/signin/feature_flags.h"
#include "ios/chrome/browser/ssl/captive_portal_features.h"
#include "ios/chrome/browser/ui/external_search/features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_features.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/web/features.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/public/features.h"
#include "ios/web/public/user_agent.h"
#include "ios/web/public/web_view_creation_util.h"

#if !defined(OFFICIAL_BUILD)
#include "components/variations/variations_switches.h"
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using flags_ui::FeatureEntry;

namespace {

const FeatureEntry::FeatureParam kMarkHttpAsDangerous[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterDangerous}};
const FeatureEntry::FeatureParam kMarkHttpAsWarning[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterWarning}};
const FeatureEntry::FeatureParam kMarkHttpAsWarningAndDangerousOnFormEdits[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::
         kMarkHttpAsParameterWarningAndDangerousOnFormEdits}};
const FeatureEntry::FeatureParam
    kMarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards[] = {
        {security_state::features::kMarkHttpAsFeatureParameterName,
         security_state::features::
             kMarkHttpAsParameterWarningAndDangerousOnPasswordsAndCreditCards}};

const FeatureEntry::FeatureVariation kMarkHttpAsFeatureVariations[] = {
    {"(mark as actively dangerous)", kMarkHttpAsDangerous,
     base::size(kMarkHttpAsDangerous), nullptr},
    {"(mark with a Not Secure warning)", kMarkHttpAsWarning,
     base::size(kMarkHttpAsWarning), nullptr},
    {"(mark with a Not Secure warning and dangerous on form edits)",
     kMarkHttpAsWarningAndDangerousOnFormEdits,
     base::size(kMarkHttpAsWarningAndDangerousOnFormEdits), nullptr},
    {"(mark with a Not Secure warning and dangerous on passwords and credit "
     "card fields)",
     kMarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards,
     base::size(kMarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards),
     nullptr}};

const FeatureEntry::Choice kUseDdljsonApiChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"(force test doodle 0)", search_provider_logos::switches::kGoogleDoodleUrl,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_ios0.json"},
    {"(force test doodle 1)", search_provider_logos::switches::kGoogleDoodleUrl,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_ios1.json"},
    {"(force test doodle 2)", search_provider_logos::switches::kGoogleDoodleUrl,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_ios2.json"},
    {"(force test doodle 3)", search_provider_logos::switches::kGoogleDoodleUrl,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_ios3.json"},
    {"(force test doodle 4)", search_provider_logos::switches::kGoogleDoodleUrl,
     "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_ios4.json"},
};

const FeatureEntry::Choice kAutofillIOSDelayBetweenFieldsChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"0", autofill::switches::kAutofillIOSDelayBetweenFields, "0"},
    {"10", autofill::switches::kAutofillIOSDelayBetweenFields, "10"},
    {"20", autofill::switches::kAutofillIOSDelayBetweenFields, "20"},
    {"50", autofill::switches::kAutofillIOSDelayBetweenFields, "50"},
    {"100", autofill::switches::kAutofillIOSDelayBetweenFields, "100"},
    {"200", autofill::switches::kAutofillIOSDelayBetweenFields, "200"},
    {"500", autofill::switches::kAutofillIOSDelayBetweenFields, "500"},
    {"1000", autofill::switches::kAutofillIOSDelayBetweenFields, "1000"},
};

const FeatureEntry::FeatureParam kIconForSearchButtonGrey[] = {
    {kIconForSearchButtonFeatureParameterName,
     kIconForSearchButtonParameterGrey}};
const FeatureEntry::FeatureParam kIconForSearchButtonColorful[] = {
    {kIconForSearchButtonFeatureParameterName,
     kIconForSearchButtonParameterColorful}};
const FeatureEntry::FeatureParam kIconForSearchButtonMagnifying[] = {
    {kIconForSearchButtonFeatureParameterName,
     kIconForSearchButtonParameterMagnifying}};

const FeatureEntry::FeatureVariation kIconForSearchButtonVariations[] = {
    {"Grey search engine logo", kIconForSearchButtonGrey,
     base::size(kIconForSearchButtonGrey), nullptr},
    {"Colorful search engine logo", kIconForSearchButtonColorful,
     base::size(kIconForSearchButtonColorful), nullptr},
    {"Magnifying glass", kIconForSearchButtonMagnifying,
     base::size(kIconForSearchButtonMagnifying), nullptr}};

// To add a new entry, add to the end of kFeatureEntries. There are four
// distinct types of entries:
// . ENABLE_DISABLE_VALUE: entry is either enabled, disabled, or uses the
//   default value for this feature. Use the ENABLE_DISABLE_VALUE_TYPE
//   macro for this type supplying the command line to the macro.
// . MULTI_VALUE: a list of choices, the first of which should correspond to a
//   deactivated state for this lab (i.e. no command line option).  To specify
//   this type of entry use the macro MULTI_VALUE_TYPE supplying it the
//   array of choices.
// . FEATURE_VALUE: entry is associated with a base::Feature instance. Entry is
//   either enabled, disabled, or uses the default value of the associated
//   base::Feature instance. To specify this type of entry use the macro
//   FEATURE_VALUE_TYPE supplying it the base::Feature instance.
// . FEATURE_WITH_PARAM_VALUES: a list of choices associated with a
//   base::Feature instance. Choices corresponding to the default state, a
//   universally enabled state, and a universally disabled state are
//   automatically included. To specify this type of entry use the macro
//   FEATURE_WITH_PARAMS_VALUE_TYPE supplying it the base::Feature instance and
//   the array of choices.
//
// See the documentation of FeatureEntry for details on the fields.
//
// When adding a new choice, add it to the end of the list.
const flags_ui::FeatureEntry kFeatureEntries[] = {
    {"enable-mark-http-as", flag_descriptions::kMarkHttpAsName,
     flag_descriptions::kMarkHttpAsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         security_state::features::kMarkHttpAsFeature,
         kMarkHttpAsFeatureVariations,
         "MarkHttpAs")},
    {"ios-captive-portal-metrics", flag_descriptions::kCaptivePortalMetricsName,
     flag_descriptions::kCaptivePortalMetricsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCaptivePortalMetrics)},
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeName,
     flag_descriptions::kInProductHelpDemoModeDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
    {"use-ddljson-api", flag_descriptions::kUseDdljsonApiName,
     flag_descriptions::kUseDdljsonApiDescription, flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kUseDdljsonApiChoices)},
    {"omnibox-ui-elide-suggestion-url-after-host",
     flag_descriptions::kOmniboxUIElideSuggestionUrlAfterHostName,
     flag_descriptions::kOmniboxUIElideSuggestionUrlAfterHostDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kUIExperimentElideSuggestionUrlAfterHost)},
    {"drag_and_drop", flag_descriptions::kDragAndDropName,
     flag_descriptions::kDragAndDropDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDragAndDrop)},
    {"external-search", flag_descriptions::kExternalSearchName,
     flag_descriptions::kExternalSearchDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kExternalSearch)},
    {"ignores-viewport-scale-limits",
     flag_descriptions::kIgnoresViewportScaleLimitsName,
     flag_descriptions::kIgnoresViewportScaleLimitsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIgnoresViewportScaleLimits)},
    {"slim-navigation-manager", flag_descriptions::kSlimNavigationManagerName,
     flag_descriptions::kSlimNavigationManagerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kSlimNavigationManager)},
    {"memex-tab-switcher", flag_descriptions::kMemexTabSwitcherName,
     flag_descriptions::kMemexTabSwitcherDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMemexTabSwitcher)},
    {"wk-http-system-cookie-store",
     flag_descriptions::kWKHTTPSystemCookieStoreName,
     flag_descriptions::kWKHTTPSystemCookieStoreName, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kWKHTTPSystemCookieStore)},
    {"enable-autofill-credit-card-upload",
     flag_descriptions::kAutofillCreditCardUploadName,
     flag_descriptions::kAutofillCreditCardUploadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUpstream)},
    {"enable-autofill-credit-card-downstream-google-pay-branding",
     flag_descriptions::kAutofillDownstreamUseGooglePayBrandingOniOSName,
     flag_descriptions::kAutofillDownstreamUseGooglePayBrandingOniOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillDownstreamUseGooglePayBrandingOniOS)},
    {"enable-autofill-credit-card-upload-google-pay-branding",
     flag_descriptions::kAutofillUpstreamUseGooglePayBrandingOnMobileName,
     flag_descriptions::
         kAutofillUpstreamUseGooglePayBrandingOnMobileDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamUseGooglePayBrandingOnMobile)},
    {"enable-autofill-save-credit-card-uses-strike-system",
     flag_descriptions::kEnableAutofillSaveCreditCardUsesStrikeSystemName,
     flag_descriptions::
         kEnableAutofillSaveCreditCardUsesStrikeSystemDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSaveCreditCardUsesStrikeSystem)},
    {"use-sync-sandbox", flag_descriptions::kSyncSandboxName,
     flag_descriptions::kSyncSandboxDescription, flags_ui::kOsIos,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
    {"wallet-service-use-sandbox",
     flag_descriptions::kWalletServiceUseSandboxName,
     flag_descriptions::kWalletServiceUseSandboxDescription, flags_ui::kOsIos,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         autofill::switches::kWalletServiceUseSandbox,
         "1",
         autofill::switches::kWalletServiceUseSandbox,
         "0")},
    {"show-autofill-type-predictions",
     flag_descriptions::kShowAutofillTypePredictionsName,
     flag_descriptions::kShowAutofillTypePredictionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillShowTypePredictions)},
    {"autofill-ios-delay-between-fields",
     flag_descriptions::kAutofillIOSDelayBetweenFieldsName,
     flag_descriptions::kAutofillIOSDelayBetweenFieldsDescription,
     flags_ui::kOsIos, MULTI_VALUE_TYPE(kAutofillIOSDelayBetweenFieldsChoices)},
    {"mailto-handling-google-ui",
     flag_descriptions::kMailtoHandlingWithGoogleUIName,
     flag_descriptions::kMailtoHandlingWithGoogleUIDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kMailtoHandledWithGoogleUI)},
    {"new-clear-browsing-data-ui",
     flag_descriptions::kNewClearBrowsingDataUIName,
     flag_descriptions::kNewClearBrowsingDataUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewClearBrowsingDataUI)},
    {"itunes-urls-store-kit-handling",
     flag_descriptions::kITunesUrlsStoreKitHandlingName,
     flag_descriptions::kITunesUrlsStoreKitHandlingDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kITunesUrlsStoreKitHandling)},
    {"unified-consent", flag_descriptions::kUnifiedConsentName,
     flag_descriptions::kUnifiedConsentDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(unified_consent::kUnifiedConsent)},
    {"force-unified-consent-bump",
     flag_descriptions::kForceUnifiedConsentBumpName,
     flag_descriptions::kForceUnifiedConsentBumpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(unified_consent::kForceUnifiedConsentBump)},
    {"autofill-dynamic-forms", flag_descriptions::kAutofillDynamicFormsName,
     flag_descriptions::kAutofillDynamicFormsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillDynamicForms)},
    {"autofill-prefilled-fields",
     flag_descriptions::kAutofillPrefilledFieldsName,
     flag_descriptions::kAutofillPrefilledFieldsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPrefilledFields)},
    {"autofill-show-all-profiles-on-prefilled-forms",
     flag_descriptions::kAutofillShowAllSuggestionsOnPrefilledFormsName,
     flag_descriptions::kAutofillShowAllSuggestionsOnPrefilledFormsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillShowAllSuggestionsOnPrefilledForms)},
    {"autofill-restrict-formless-form-extraction",
     flag_descriptions::kAutofillRestrictUnownedFieldsToFormlessCheckoutName,
     flag_descriptions::
         kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillRestrictUnownedFieldsToFormlessCheckout)},
    {"fullscreen-viewport-adjustment-experiment",
     flag_descriptions::kFullscreenViewportAdjustmentExperimentName,
     flag_descriptions::kFullscreenViewportAdjustmentExperimentDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(
         fullscreen::features::kViewportAdjustmentExperimentChoices)},
    {"autofill-enforce-min-required-fields-for-heuristics",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForHeuristicsName,
     flag_descriptions::
         kAutofillEnforceMinRequiredFieldsForHeuristicsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForHeuristics)},
    {"autofill-enforce-min-required-fields-for-query",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForQueryName,
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForQueryDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForQuery)},
    {"autofill-enforce-min-required-fields-for-upload",
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForUploadName,
     flag_descriptions::kAutofillEnforceMinRequiredFieldsForUploadDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceMinRequiredFieldsForUpload)},
    {"browser-container-fullscreen",
     flag_descriptions::kBrowserContainerFullscreenName,
     flag_descriptions::kBrowserContainerFullscreenDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kBrowserContainerFullscreen)},
    {"autofill-cache-query-responses",
     flag_descriptions::kAutofillCacheQueryResponsesName,
     flag_descriptions::kAutofillCacheQueryResponsesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCacheQueryResponses)},
    {"autofill-enable-company-name",
     flag_descriptions::kAutofillEnableCompanyNameName,
     flag_descriptions::kAutofillEnableCompanyNameDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCompanyName)},
    {"autofill-manual-fallback", flag_descriptions::kAutofillManualFallbackName,
     flag_descriptions::kAutofillManualFallbackDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillManualFallback)},
    {"webpage-text-accessibility",
     flag_descriptions::kWebPageTextAccessibilityName,
     flag_descriptions::kWebPageTextAccessibilityDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageTextAccessibility)},
    {"web-frame-messaging", flag_descriptions::kWebFrameMessagingName,
     flag_descriptions::kWebFrameMessagingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kWebFrameMessaging)},
    {"copy-image", flag_descriptions::kCopyImageName,
     flag_descriptions::kCopyImageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCopyImage)},
    {"new-password-form-parsing",
     flag_descriptions::kNewPasswordFormParsingName,
     flag_descriptions::kNewPasswordFormParsingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kNewPasswordFormParsing)},
    {"app-launcher-refresh", flag_descriptions::kAppLauncherRefreshName,
     flag_descriptions::kAppLauncherRefreshDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAppLauncherRefresh)},
    {"sync-standalone-transport",
     flag_descriptions::kSyncStandaloneTransportName,
     flag_descriptions::kSyncStandaloneTransportDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kSyncStandaloneTransport)},
    {"sync-support-secondary-account",
     flag_descriptions::kSyncSupportSecondaryAccountName,
     flag_descriptions::kSyncSupportSecondaryAccountDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kSyncSupportSecondaryAccount)},
    {"out-of-web-fullscreen", flag_descriptions::kOutOfWebFullscreenName,
     flag_descriptions::kOutOfWebFullscreenDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kOutOfWebFullscreen)},
    {"autofill-manual-fallback-phase-two",
     flag_descriptions::kAutofillManualFallbackPhaseTwoName,
     flag_descriptions::kAutofillManualFallbackPhaseTwoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillManualFallbackPhaseTwo)},
    {"toolbar-container", flag_descriptions::kToolbarContainerName,
     flag_descriptions::kToolbarContainerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(toolbar_container::kToolbarContainerEnabled)},
    {"omnibox-popup-shortcuts",
     flag_descriptions::kOmniboxPopupShortcutIconsInZeroStateName,
     flag_descriptions::kOmniboxPopupShortcutIconsInZeroStateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPopupShortcutIconsInZeroState)},
    {"sso-with-wkwebview", flag_descriptions::kSSOWithWKWebViewName,
     flag_descriptions::kSSOWithWKWebViewDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSSOWithWKWebView)},
    {"wk-web-view-snapshots", flag_descriptions::kWKWebViewSnapshotsName,
     flag_descriptions::kWKWebViewSnapshotsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kWKWebViewSnapshots)},
    {"custom-search-engines", flag_descriptions::kCustomSearchEnginesName,
     flag_descriptions::kCustomSearchEnginesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCustomSearchEngines)},
    {"use-multilogin-endpoint", flag_descriptions::kUseMultiloginEndpointName,
     flag_descriptions::kUseMultiloginEndpointDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseMultiloginEndpoint)},
    {"closing-last-incognito-tab",
     flag_descriptions::kClosingLastIncognitoTabName,
     flag_descriptions::kClosingLastIncognitoTabDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kClosingLastIncognitoTab)},
    {"omnibox-tab-switch-suggestions",
     flag_descriptions::kOmniboxTabSwitchSuggestionsName,
     flag_descriptions::kOmniboxTabSwitchSuggestionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxTabSwitchSuggestions)},
    {"fcm-invalidations", flag_descriptions::kFCMInvalidationsName,
     flag_descriptions::kFCMInvalidationsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(invalidation::switches::kFCMInvalidations)},
    {"browser-container-contains-ntp",
     flag_descriptions::kBrowserContainerContainsNTPName,
     flag_descriptions::kBrowserContainerContainsNTPDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kBrowserContainerContainsNTP)},
    {"search-icon-toggle", flag_descriptions::kSearchIconToggleName,
     flag_descriptions::kSearchIconToggleDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIconForSearchButtonFeature,
                                    kIconForSearchButtonVariations,
                                    "ToggleSearchButtonIcon")},
};

// Add all switches from experimental flags to |command_line|.
void AppendSwitchesFromExperimentalSettings(base::CommandLine* command_line) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Set the UA flag if UseMobileSafariUA is enabled.
  if ([defaults boolForKey:@"UseMobileSafariUA"]) {
    // Safari uses "Vesion/", followed by the OS version excluding bugfix, where
    // Chrome puts its product token.
    int32_t major = 0;
    int32_t minor = 0;
    int32_t bugfix = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    std::string product = base::StringPrintf("Version/%d.%d", major, minor);

    command_line->AppendSwitchASCII(switches::kUserAgent,
                                    web::BuildUserAgentFromProduct(product));
  }

  // Freeform commandline flags.  These are added last, so that any flags added
  // earlier in this function take precedence.
  if ([defaults boolForKey:@"EnableFreeformCommandLineFlags"]) {
    base::CommandLine::StringVector flags;
    // Append an empty "program" argument.
    flags.push_back("");

    // The number of flags corresponds to the number of text fields in
    // Experimental.plist.
    const int kNumFreeformFlags = 5;
    for (int i = 1; i <= kNumFreeformFlags; ++i) {
      NSString* key =
          [NSString stringWithFormat:@"FreeformCommandLineFlag%d", i];
      NSString* flag = [defaults stringForKey:key];
      if ([flag length]) {
        flags.push_back(base::SysNSStringToUTF8(flag));
      }
    }

    base::CommandLine temp_command_line(flags);
    command_line->AppendArguments(temp_command_line, false);
  }

  // Populate command line flag for 3rd party keyboard omnibox workaround.
  NSString* enableThirdPartyKeyboardWorkaround =
      [defaults stringForKey:@"EnableThirdPartyKeyboardWorkaround"];
  if ([enableThirdPartyKeyboardWorkaround isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableThirdPartyKeyboardWorkaround);
  } else if ([enableThirdPartyKeyboardWorkaround isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableThirdPartyKeyboardWorkaround);
  }

  ios::GetChromeBrowserProvider()->AppendSwitchesFromExperimentalSettings(
      defaults, command_line);
}

bool SkipConditionalFeatureEntry(const flags_ui::FeatureEntry& entry) {
  return false;
}

class FlagsStateSingleton {
 public:
  FlagsStateSingleton()
      : flags_state_(kFeatureEntries, base::size(kFeatureEntries)) {}
  ~FlagsStateSingleton() {}

  static FlagsStateSingleton* GetInstance() {
    return base::Singleton<FlagsStateSingleton>::get();
  }

  static flags_ui::FlagsState* GetFlagsState() {
    return &GetInstance()->flags_state_;
  }

 private:
  flags_ui::FlagsState flags_state_;

  DISALLOW_COPY_AND_ASSIGN(FlagsStateSingleton);
};
}  // namespace

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line) {
  AppendSwitchesFromExperimentalSettings(command_line);
  FlagsStateSingleton::GetFlagsState()->ConvertFlagsToSwitches(
      flags_storage, command_line, flags_ui::kAddSentinels,
      switches::kEnableFeatures, switches::kDisableFeatures);
}

std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list) {
  return FlagsStateSingleton::GetFlagsState()
      ->RegisterAllFeatureVariationParameters(flags_storage, feature_list);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries) {
  FlagsStateSingleton::GetFlagsState()->GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::Bind(&SkipConditionalFeatureEntry));
}

void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable) {
  FlagsStateSingleton::GetFlagsState()->SetFeatureEntryEnabled(
      flags_storage, internal_name, enable);
}

void ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
  FlagsStateSingleton::GetFlagsState()->ResetAllFlags(flags_storage);
}

namespace testing {

const flags_ui::FeatureEntry* GetFeatureEntries(size_t* count) {
  *count = base::size(kFeatureEntries);
  return kFeatureEntries;
}

}  // namespace testing
