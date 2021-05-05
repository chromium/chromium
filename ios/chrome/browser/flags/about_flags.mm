// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of about_flags for iOS that sets flags based on experimental
// settings.

#include "ios/chrome/browser/flags/about_flags.h"

#import <UIKit/UIKit.h>
#include <stddef.h>
#include <stdint.h>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#include "components/breadcrumbs/core/features.h"
#include "components/content_settings/core/common/features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/ntp_tiles/switches.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "components/policy/policy_constants.h"
#include "components/security_state/core/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/ios/browser/features.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/ukm/ios/features.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/crash_report/features.h"
#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#include "ios/chrome/browser/policy/policy_features.h"
#include "ios/chrome/browser/screen_time/screen_time_buildflags.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_features.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/web/features.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/common/features.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/common/web_view_creation_util.h"

#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
#include "ios/chrome/browser/screen_time/features.h"
#endif

#if !defined(OFFICIAL_BUILD)
#include "components/variations/variations_switches.h"
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using flags_ui::FeatureEntry;

namespace {

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

const FeatureEntry::Choice kDelayThresholdMinutesToUpdateGaiaCookieChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"0", signin::kDelayThresholdMinutesToUpdateGaiaCookie, "0"},
    {"10", signin::kDelayThresholdMinutesToUpdateGaiaCookie, "10"},
    {"60", signin::kDelayThresholdMinutesToUpdateGaiaCookie, "60"},
};

const FeatureEntry::FeatureVariation
    kOmniboxOnDeviceHeadSuggestNonIncognitoExperimentVariations[] = {
        {
            "relevance-1000",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
                 "1000"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDemoteMode,
                 "decrease-relevances"}},
            2,
            nullptr,
        },
        {
            "no-delay-relevance-1000",
            (FeatureEntry::FeatureParam[]){
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs,
                 "0"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestMaxScoreForNonUrlInput,
                 "1000"},
                {OmniboxFieldTrial::kOnDeviceHeadSuggestDemoteMode,
                 "decrease-relevances"}},
            3,
            nullptr,
        }};

const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches3[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "3"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches4[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "4"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches5[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "5"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches6[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches8[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "8"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches10[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "10"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches12[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "12"}};

const FeatureEntry::FeatureVariation
    kOmniboxUIMaxAutocompleteMatchesVariations[] = {
        {"3 matches", kOmniboxUIMaxAutocompleteMatches3,
         base::size(kOmniboxUIMaxAutocompleteMatches3), nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4,
         base::size(kOmniboxUIMaxAutocompleteMatches4), nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5,
         base::size(kOmniboxUIMaxAutocompleteMatches5), nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6,
         base::size(kOmniboxUIMaxAutocompleteMatches6), nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8,
         base::size(kOmniboxUIMaxAutocompleteMatches8), nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10,
         base::size(kOmniboxUIMaxAutocompleteMatches10), nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12,
         base::size(kOmniboxUIMaxAutocompleteMatches12), nullptr}};

const FeatureEntry::FeatureParam
    kAutofillUseMobileLabelDisambiguationShowAll[] = {
        {autofill::features::kAutofillUseMobileLabelDisambiguationParameterName,
         autofill::features::
             kAutofillUseMobileLabelDisambiguationParameterShowAll}};
const FeatureEntry::FeatureParam
    kAutofillUseMobileLabelDisambiguationShowOne[] = {
        {autofill::features::kAutofillUseMobileLabelDisambiguationParameterName,
         autofill::features::
             kAutofillUseMobileLabelDisambiguationParameterShowOne}};

const FeatureEntry::FeatureVariation
    kAutofillUseMobileLabelDisambiguationVariations[] = {
        {"(show all)", kAutofillUseMobileLabelDisambiguationShowAll,
         base::size(kAutofillUseMobileLabelDisambiguationShowAll), nullptr},
        {"(show one)", kAutofillUseMobileLabelDisambiguationShowOne,
         base::size(kAutofillUseMobileLabelDisambiguationShowOne), nullptr}};

const FeatureEntry::FeatureParam
    kDefaultBrowserFullscreenPromoCTAExperimentSwitch[] = {
        {kDefaultBrowserFullscreenPromoCTAExperimentSwitchParam, "true"}};
const FeatureEntry::FeatureParam
    kDefaultBrowserFullscreenPromoCTAExperimentOpenLinks[] = {
        {kDefaultBrowserFullscreenPromoCTAExperimentOpenLinksParam, "true"}};
const FeatureEntry::FeatureVariation
    kDefaultBrowserFullscreenPromoCTAExperimentVariations[] = {
        {"Switch to Chrome", kDefaultBrowserFullscreenPromoCTAExperimentSwitch,
         base::size(kDefaultBrowserFullscreenPromoCTAExperimentSwitch),
         nullptr},
        {"Open Links in Chrome",
         kDefaultBrowserFullscreenPromoCTAExperimentOpenLinks,
         base::size(kDefaultBrowserFullscreenPromoCTAExperimentOpenLinks),
         nullptr}};

const FeatureEntry::FeatureParam kDefaultPromoTailoredIOS[] = {
    {kDefaultPromoTailoredVariantIOSParam, "true"}};
const FeatureEntry::FeatureParam kDefaultPromoTailoredSafe[] = {
    {kDefaultPromoTailoredVariantSafeParam, "true"}};
const FeatureEntry::FeatureParam kDefaultPromoTailoredTabs[] = {
    {kDefaultPromoTailoredVariantTabsParam, "true"}};
const FeatureEntry::FeatureVariation kDefaultPromoTailoredVariations[] = {
    {"Built for iOS", kDefaultPromoTailoredIOS,
     base::size(kDefaultPromoTailoredIOS), nullptr},
    {"Stay Safe With Google Chrome", kDefaultPromoTailoredSafe,
     base::size(kDefaultPromoTailoredSafe), nullptr},
    {"All Your Tabs In One Browser", kDefaultPromoTailoredTabs,
     base::size(kDefaultPromoTailoredTabs), nullptr},
};

const FeatureEntry::FeatureParam
    kDefaultPromoNonModalShortTimeoutWithInstructions[] = {
        {kDefaultPromoNonModalTimeoutParam, "15"},
        {kDefaultPromoNonModalInstructionsParam, "true"}};
const FeatureEntry::FeatureParam
    kDefaultPromoNonModalLongTimeoutWithInstructions[] = {
        {kDefaultPromoNonModalTimeoutParam, "45"},
        {kDefaultPromoNonModalInstructionsParam, "true"}};
const FeatureEntry::FeatureParam
    kDefaultPromoNonModalShortTimeoutWithoutInstructions[] = {
        {kDefaultPromoNonModalTimeoutParam, "15"}};
const FeatureEntry::FeatureParam
    kDefaultPromoNonModalLongTimeoutWithoutInstructions[] = {
        {kDefaultPromoNonModalTimeoutParam, "45"}};
const FeatureEntry::FeatureVariation kDefaultPromoNonModalVariations[] = {
    {"Short timeout, with instructions",
     kDefaultPromoNonModalShortTimeoutWithInstructions,
     base::size(kDefaultPromoNonModalShortTimeoutWithInstructions), nullptr},
    {"Long timeout, with instructions",
     kDefaultPromoNonModalLongTimeoutWithInstructions,
     base::size(kDefaultPromoNonModalLongTimeoutWithInstructions), nullptr},
    {"Short timeout, without instructions",
     kDefaultPromoNonModalShortTimeoutWithoutInstructions,
     base::size(kDefaultPromoNonModalShortTimeoutWithoutInstructions), nullptr},
    {"Long timeout, without instructions",
     kDefaultPromoNonModalLongTimeoutWithoutInstructions,
     base::size(kDefaultPromoNonModalLongTimeoutWithoutInstructions), nullptr},
};

const FeatureEntry::FeatureParam kDiscoverFeedInNtpEnableNativeUI[] = {
    {kDiscoverFeedIsNativeUIEnabled, "true"}};
const FeatureEntry::FeatureVariation kDiscoverFeedInNtpVariations[] = {
    {"Native UI", kDiscoverFeedInNtpEnableNativeUI,
     base::size(kDiscoverFeedInNtpEnableNativeUI), nullptr}};

const FeatureEntry::FeatureParam kRefactoredNTPLogging[] = {
    {kRefactoredNTPLoggingEnabled, "true"}};
const FeatureEntry::FeatureVariation kRefactoredNTPLoggingVariations[] = {
    {"Logging Enabled", kRefactoredNTPLogging,
     base::size(kRefactoredNTPLogging), nullptr}};

const FeatureEntry::FeatureParam kStartSurfaceTenSecondsShrinkLogo[] = {
    {kStartSurfaceShrinkLogoParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam kStartSurfaceTenSecondsHideShortcuts[] = {
    {kStartSurfaceHideShortcutsParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam kStartSurfaceTenSecondsReturnToRecentTab[] = {
    {kStartSurfaceReturnToRecentTabParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam
    kStartSurfaceTenSecondsShrinkLogoReturnToRecentTab[] = {
        {kStartSurfaceShrinkLogoParam, "true"},
        {kStartSurfaceReturnToRecentTabParam, "true"},
        {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam
    kStartSurfaceTenSecondsHideShortcutsReturnToRecentTab[] = {
        {kStartSurfaceHideShortcutsParam, "true"},
        {kStartSurfaceReturnToRecentTabParam, "true"},
        {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam kStartSurfaceOneHourShrinkLogo[] = {
    {kStartSurfaceShrinkLogoParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};
const FeatureEntry::FeatureParam kStartSurfaceOneHourHideShortcuts[] = {
    {kStartSurfaceHideShortcutsParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};
const FeatureEntry::FeatureParam kStartSurfaceOneHourReturnToRecentTab[] = {
    {kStartSurfaceReturnToRecentTabParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};
const FeatureEntry::FeatureParam
    kStartSurfaceOneHourShrinkLogoReturnToRecentTab[] = {
        {kStartSurfaceShrinkLogoParam, "true"},
        {kStartSurfaceReturnToRecentTabParam, "true"},
        {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};
const FeatureEntry::FeatureParam
    kStartSurfaceOneHourHideShortcutsReturnToRecentTab[] = {
        {kStartSurfaceHideShortcutsParam, "true"},
        {kStartSurfaceReturnToRecentTabParam, "true"},
        {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};

const FeatureEntry::FeatureVariation kStartSurfaceVariations[] = {
    {"10s:Show Return to Recent Tab tile",
     kStartSurfaceTenSecondsReturnToRecentTab,
     base::size(kStartSurfaceTenSecondsReturnToRecentTab), nullptr},
    {"10s:Shrink Logo", kStartSurfaceTenSecondsShrinkLogo,
     base::size(kStartSurfaceTenSecondsShrinkLogo), nullptr},
    {"10s:Hide Shortcuts", kStartSurfaceTenSecondsHideShortcuts,
     base::size(kStartSurfaceTenSecondsHideShortcuts), nullptr},
    {"10s:Shrink Logo and show Return to Recent Tab tile",
     kStartSurfaceTenSecondsShrinkLogoReturnToRecentTab,
     base::size(kStartSurfaceTenSecondsShrinkLogoReturnToRecentTab), nullptr},
    {"10s:Hide Shortcuts and show Return to Recent Tab tile",
     kStartSurfaceTenSecondsHideShortcutsReturnToRecentTab,
     base::size(kStartSurfaceTenSecondsHideShortcutsReturnToRecentTab),
     nullptr},
    {"1h:Show Return to Recent Tab tile", kStartSurfaceOneHourReturnToRecentTab,
     base::size(kStartSurfaceOneHourReturnToRecentTab), nullptr},
    {"1h:Shrink Logo", kStartSurfaceOneHourShrinkLogo,
     base::size(kStartSurfaceOneHourShrinkLogo), nullptr},
    {"1h:Hide Shortcuts", kStartSurfaceOneHourHideShortcuts,
     base::size(kStartSurfaceOneHourHideShortcuts), nullptr},
    {"1h:Shrink Logo and show Return to Recent Tab tile",
     kStartSurfaceOneHourShrinkLogoReturnToRecentTab,
     base::size(kStartSurfaceOneHourShrinkLogoReturnToRecentTab), nullptr},
    {"1h:Hide Shortcuts and show Return to Recent Tab tile",
     kStartSurfaceOneHourHideShortcutsReturnToRecentTab,
     base::size(kStartSurfaceOneHourHideShortcutsReturnToRecentTab), nullptr},
};

const FeatureEntry::FeatureParam kWebViewNativeContextMenuWeb[] = {
    {web::features::kWebViewNativeContextMenuName,
     web::features::kWebViewNativeContextMenuParameterWeb}};
const FeatureEntry::FeatureParam kWebViewNativeContextMenuSystem[] = {
    {web::features::kWebViewNativeContextMenuName,
     web::features::kWebViewNativeContextMenuParameterSystem}};

const FeatureEntry::FeatureVariation kWebViewNativeContextMenuVariations[] = {
    {"Web", kWebViewNativeContextMenuWeb,
     base::size(kWebViewNativeContextMenuWeb), nullptr},
    {"System", kWebViewNativeContextMenuSystem,
     base::size(kWebViewNativeContextMenuSystem), nullptr}};

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
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeName,
     flag_descriptions::kInProductHelpDemoModeDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
    {"enable-autofill-credit-card-upload",
     flag_descriptions::kAutofillCreditCardUploadName,
     flag_descriptions::kAutofillCreditCardUploadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUpstream)},
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
    {"fullscreen-viewport-adjustment-experiment",
     flag_descriptions::kFullscreenSmoothScrollingName,
     flag_descriptions::kFullscreenSmoothScrollingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(fullscreen::features::kSmoothScrollingDefault)},
    {"webpage-default-zoom-from-dynamic-type",
     flag_descriptions::kWebPageDefaultZoomFromDynamicTypeName,
     flag_descriptions::kWebPageDefaultZoomFromDynamicTypeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageDefaultZoomFromDynamicType)},
    {"webpage-text-accessibility",
     flag_descriptions::kWebPageTextAccessibilityName,
     flag_descriptions::kWebPageTextAccessibilityDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageTextAccessibility)},
    {"webpage-alternative-text-zoom",
     flag_descriptions::kWebPageAlternativeTextZoomName,
     flag_descriptions::kWebPageAlternativeTextZoomDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(web::kWebPageAlternativeTextZoom)},
    {"toolbar-container", flag_descriptions::kToolbarContainerName,
     flag_descriptions::kToolbarContainerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(toolbar_container::kToolbarContainerEnabled)},
    {"omnibox-on-device-head-suggestions-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsIncognitoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOnDeviceHeadProviderIncognito)},
    {"omnibox-on-device-head-suggestions-non-incognito",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsNonIncognitoName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kOnDeviceHeadProviderNonIncognito,
         kOmniboxOnDeviceHeadSuggestNonIncognitoExperimentVariations,
         "OmniboxOnDeviceHeadNonIncognitoTuningMobile")},
    {"omnibox-ui-max-autocomplete-matches",
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesName,
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kUIExperimentMaxAutocompleteMatches,
         kOmniboxUIMaxAutocompleteMatchesVariations,
         "OmniboxUIMaxAutocompleteVariations")},
    {"omnibox-local-history-zero-suggest",
     flag_descriptions::kOmniboxLocalHistoryZeroSuggestName,
     flag_descriptions::kOmniboxLocalHistoryZeroSuggestDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kLocalHistoryZeroSuggest)},
#if defined(DCHECK_IS_CONFIGURABLE)
    {"dcheck-is-fatal", flag_descriptions::kDcheckIsFatalName,
     flag_descriptions::kDcheckIsFatalDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(base::kDCheckIsFatalFeature)},
#endif  // defined(DCHECK_IS_CONFIGURABLE)
    {"settings-refresh", flag_descriptions::kSettingsRefreshName,
     flag_descriptions::kSettingsRefreshDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSettingsRefresh)},
    {"send-uma-cellular", flag_descriptions::kSendUmaOverAnyNetwork,
     flag_descriptions::kSendUmaOverAnyNetworkDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUmaCellular)},
    {"autofill-use-mobile-label-disambiguation",
     flag_descriptions::kAutofillUseMobileLabelDisambiguationName,
     flag_descriptions::kAutofillUseMobileLabelDisambiguationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUseMobileLabelDisambiguation,
         kAutofillUseMobileLabelDisambiguationVariations,
         "AutofillUseMobileLabelDisambiguation")},
    {"autofill-prune-suggestions",
     flag_descriptions::kAutofillPruneSuggestionsName,
     flag_descriptions::kAutofillPruneSuggestionsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPruneSuggestions)},
    {"collections-card-presentation-style",
     flag_descriptions::kCollectionsCardPresentationStyleName,
     flag_descriptions::kCollectionsCardPresentationStyleDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCollectionsCardPresentationStyle)},
    {"ios-breadcrumbs", flag_descriptions::kLogBreadcrumbsName,
     flag_descriptions::kLogBreadcrumbsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(breadcrumbs::kLogBreadcrumbs)},
    {"ios-synthetic-crash-reports",
     flag_descriptions::kSyntheticCrashReportsForUteName,
     flag_descriptions::kSyntheticCrashReportsForUteDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kSyntheticCrashReportsForUte)},
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(signin::kForceStartupSigninPromo)},
    {"restore-session-from-cache",
     flag_descriptions::kRestoreSessionFromCacheName,
     flag_descriptions::kRestoreSessionFromCacheDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kRestoreSessionFromCache)},
    {"autofill-save-card-dismiss-on-navigation",
     flag_descriptions::kAutofillSaveCardDismissOnNavigationName,
     flag_descriptions::kAutofillSaveCardDismissOnNavigationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSaveCardDismissOnNavigation)},
    {"force-unstacked-tabstrip", flag_descriptions::kForceUnstackedTabstripName,
     flag_descriptions::kForceUnstackedTabstripDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kForceUnstackedTabstrip)},
    {"use-js-error-page", flag_descriptions::kUseJSForErrorPageName,
     flag_descriptions::kUseJSForErrorPageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kUseJSForErrorPage)},
    {"desktop-version-default", flag_descriptions::kDefaultToDesktopOnIPadName,
     flag_descriptions::kDefaultToDesktopOnIPadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kUseDefaultUserAgentInWebClient)},
    {"mobile-google-srp", flag_descriptions::kMobileGoogleSRPName,
     flag_descriptions::kMobileGoogleSRPDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kMobileGoogleSRP)},
    {"infobar-overlay-ui", flag_descriptions::kInfobarOverlayUIName,
     flag_descriptions::kInfobarOverlayUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kInfobarOverlayUI)},
    {"url-blocklist-ios", flag_descriptions::kURLBlocklistIOSName,
     flag_descriptions::kURLBlocklistIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kURLBlocklistIOS)},
    {"enable-ios-managed-settings-ui",
     flag_descriptions::kEnableIOSManagedSettingsUIName,
     flag_descriptions::kEnableIOSManagedSettingsUIDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableIOSManagedSettingsUI)},
    {"new-content-suggestions-feed", flag_descriptions::kDiscoverFeedInNtpName,
     flag_descriptions::kDiscoverFeedInNtpDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDiscoverFeedInNtp,
                                    kDiscoverFeedInNtpVariations,
                                    "IOSDiscoverFeed")},
    {"refactored-ntp", flag_descriptions::kRefactoredNTPName,
     flag_descriptions::kRefactoredNTPDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kRefactoredNTP,
                                    kRefactoredNTPLoggingVariations,
                                    "RefactoredNTP")},
    {"illustrated-empty-states", flag_descriptions::kIllustratedEmptyStatesName,
     flag_descriptions::kIllustratedEmptyStatesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIllustratedEmptyStates)},
    {"expanded-tab-strip", flag_descriptions::kExpandedTabStripName,
     flag_descriptions::kExpandedTabStripDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kExpandedTabStrip)},
    {"autofill-enable-offers-in-downstream",
     flag_descriptions::kAutofillEnableOffersInDownstreamName,
     flag_descriptions::kAutofillEnableOffersInDownstreamDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableOffersInDownstream)},
    {"shared-highlighting-ios", flag_descriptions::kSharedHighlightingIOSName,
     flag_descriptions::kSharedHighlightingIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSharedHighlightingIOS)},
    {"enable-fre-ui-module-ios", flag_descriptions::kEnableFREUIModuleIOSName,
     flag_descriptions::kEnableFREUIModuleIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFREUIModuleIOS)},
    {"enable-fullpage-screenshot",
     flag_descriptions::kEnableFullPageScreenshotName,
     flag_descriptions::kEnableFullPageScreenshotDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFullPageScreenshot)},
    {"legacy-tls-interstitial",
     flag_descriptions::kIOSLegacyTLSInterstitialsName,
     flag_descriptions::kIOSLegacyTLSInterstitialsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIOSLegacyTLSInterstitial)},
    {"enable-close-all-tabs-confirmation",
     flag_descriptions::kEnableCloseAllTabsConfirmationName,
     flag_descriptions::kEnableCloseAllTabsConfirmationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableCloseAllTabsConfirmation)},
#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
    {"screen-time-integration-ios",
     flag_descriptions::kScreenTimeIntegrationName,
     flag_descriptions::kScreenTimeIntegrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kScreenTimeIntegration)},
#endif
    {"mobile-identity-consistency",
     flag_descriptions::kMobileIdentityConsistencyName,
     flag_descriptions::kMobileIdentityConsistencyDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(signin::kMobileIdentityConsistency)},
    {"simplify-sign-out-ios", flag_descriptions::kSimplifySignOutIOSName,
     flag_descriptions::kSimplifySignOutIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(signin::kSimplifySignOutIOS)},
    {"default-browser-setting", flag_descriptions::kDefaultBrowserSettingsName,
     flag_descriptions::kDefaultBrowserSettingsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDefaultBrowserSettings)},
    {"modern-tab-strip", flag_descriptions::kModernTabStripName,
     flag_descriptions::kModernTabStripDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kModernTabStrip)},
    {"autofill-use-renderer-ids",
     flag_descriptions::kAutofillUseRendererIDsName,
     flag_descriptions::kAutofillUseRendererIDsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUseUniqueRendererIDsOnIOS)},
    {"restore-gaia-cookies-on-user-action",
     flag_descriptions::kRestoreGaiaCookiesOnUserActionName,
     flag_descriptions::kRestoreGaiaCookiesOnUserActionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(signin::kRestoreGaiaCookiesOnUserAction)},
    {"use-username-for-signin-notification-infobar-title",
     flag_descriptions::kSigninNotificationInfobarUsernameInTitleName,
     flag_descriptions::kSigninNotificationInfobarUsernameInTitleDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(signin::kSigninNotificationInfobarUsernameInTitle)},
    {"minutes-delay-to-restore-gaia-cookies-if-deleted",
     flag_descriptions::kDelayThresholdMinutesToUpdateGaiaCookieName,
     flag_descriptions::kDelayThresholdMinutesToUpdateGaiaCookieDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kDelayThresholdMinutesToUpdateGaiaCookieChoices)},
    {"edit-passwords-in-settings",
     flag_descriptions::kEditPasswordsInSettingsName,
     flag_descriptions::kEditPasswordsInSettingsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kEditPasswordsInSettings)},
    {"enable-incognito-authentication-ios",
     flag_descriptions::kIncognitoAuthenticationName,
     flag_descriptions::kIncognitoAuthenticationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIncognitoAuthentication)},
    {"web-view-native-context-menu",
     flag_descriptions::kWebViewNativeContextMenuName,
     flag_descriptions::kWebViewNativeContextMenuDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(web::features::kWebViewNativeContextMenu,
                                    kWebViewNativeContextMenuVariations,
                                    "WebViewNativeContextMenu")},
    {"location-permissions-prompt",
     flag_descriptions::kLocationPermissionsPromptName,
     flag_descriptions::kLocationPermissionsPromptDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLocationPermissionsPrompt)},
    {"record-snapshot-size", flag_descriptions::kRecordSnapshotSizeName,
     flag_descriptions::kRecordSnapshotSizeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kRecordSnapshotSize)},
    {"default-browser-fullscreen-promo-experiment",
     flag_descriptions::kDefaultBrowserFullscreenPromoExperimentName,
     flag_descriptions::kDefaultBrowserFullscreenPromoExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDefaultBrowserFullscreenPromoExperiment)},
    {"ios-shared-highlighting-color-change",
     flag_descriptions::kIOSSharedHighlightingColorChangeName,
     flag_descriptions::kIOSSharedHighlightingColorChangeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIOSSharedHighlightingColorChange)},
    {"ios-persist-crash-restore-infobar",
     flag_descriptions::kIOSPersistCrashRestoreName,
     flag_descriptions::kIOSPersistCrashRestoreDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSPersistCrashRestore)},
    {"change-password-affiliation",
     flag_descriptions::kChangePasswordAffiliationInfoName,
     flag_descriptions::kChangePasswordAffiliationInfoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kChangePasswordAffiliationInfo)},
    {"use-of-hash-affiliation-fetcher",
     flag_descriptions::kUseOfHashAffiliationFetcherName,
     flag_descriptions::kUseOfHashAffiliationFetcherDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kUseOfHashAffiliationFetcher)},
    {"omnibox-new-textfield-implementation",
     flag_descriptions::kOmniboxNewImplementationName,
     flag_descriptions::kOmniboxNewImplementationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSNewOmniboxImplementation)},
    {"shared-highlighting-use-blocklist",
     flag_descriptions::kSharedHighlightingUseBlocklistIOSName,
     flag_descriptions::kSharedHighlightingUseBlocklistIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingUseBlocklist)},
    {"start-surface", flag_descriptions::kStartSurfaceName,
     flag_descriptions::kStartSurfaceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kStartSurface,
                                    kStartSurfaceVariations,
                                    "StartSurface")},
    {"ios-crashpad", flag_descriptions::kCrashpadIOSName,
     flag_descriptions::kCrashpadIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCrashpadIOS)},
    {"detect-form-submission-on-form-clear",
     flag_descriptions::kDetectFormSubmissionOnFormClearIOSName,
     flag_descriptions::kDetectFormSubmissionOnFormClearIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kDetectFormSubmissionOnFormClear)},
    {"default-browser-fullscreen-promo-cta-experiment",
     flag_descriptions::kDefaultBrowserFullscreenPromoCTAExperimentName,
     flag_descriptions::kDefaultBrowserFullscreenPromoCTAExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kDefaultBrowserFullscreenPromoCTAExperiment,
         kDefaultBrowserFullscreenPromoCTAExperimentVariations,
         "DefaultBrowserFullscreenPromoCTAExperiment")},
    {"password-reuse-detection", flag_descriptions::kPasswordReuseDetectionName,
     flag_descriptions::kPasswordReuseDetectionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordReuseDetectionEnabled)},
    {"enable-manual-password-generation",
     flag_descriptions::kEnableManualPasswordGenerationName,
     flag_descriptions::kEnableManualPasswordGenerationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnableManualPasswordGeneration)},
    {"interest-feed-notice-card-auto-dismiss",
     flag_descriptions::kInterestFeedNoticeCardAutoDismissName,
     flag_descriptions::kInterestFeedNoticeCardAutoDismissDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(feed::kInterestFeedNoticeCardAutoDismiss)},
    {"autofill-address-save-prompt",
     flag_descriptions::kEnableAutofillAddressSavePromptName,
     flag_descriptions::kEnableAutofillAddressSavePromptDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAddressProfileSavePrompt)},
    {"filling-across-affiliated-websites",
     flag_descriptions::kFillingAcrossAffiliatedWebsitesName,
     flag_descriptions::kFillingAcrossAffiliatedWebsitesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kFillingAcrossAffiliatedWebsites)},
    {"default-browser-promo-non-modal",
     flag_descriptions::kDefaultPromoNonModalName,
     flag_descriptions::kDefaultPromoNonModalDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDefaultPromoNonModal,
                                    kDefaultPromoNonModalVariations,
                                    "DefaultPromoNonModal")},
    {"default-browser-promo-tailored",
     flag_descriptions::kDefaultPromoTailoredName,
     flag_descriptions::kDefaultPromoTailoredDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDefaultPromoTailored,
                                    kDefaultPromoTailoredVariations,
                                    "DefaultPromoTailored")},
    {"autofill-parse-merchant-promo-code-fields",
     flag_descriptions::kAutofillParseMerchantPromoCodeFieldsName,
     flag_descriptions::kAutofillParseMerchantPromoCodeFieldsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillParseMerchantPromoCodeFields)},
    {"search-history-link-ios", flag_descriptions::kSearchHistoryLinkIOSName,
     flag_descriptions::kSearchHistoryLinkIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSearchHistoryLinkIOS)},
    {"interest-feed-v2-clicks-and-views-cond-upload",
     flag_descriptions::kInterestFeedV2ClickAndViewActionsConditionalUploadName,
     flag_descriptions::
         kInterestFeedV2ClickAndViewActionsConditionalUploadDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2ClicksAndViewsConditionalUpload)},
    {"tabs-bulkactions-ios", flag_descriptions::kTabsBulkActionsName,
     flag_descriptions::kTabsBulkActionsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabsBulkActions)},
    {"tabgrid-context-menu-ios", flag_descriptions::kTabGridContextMenuName,
     flag_descriptions::kTabGridContextMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridContextMenu)},
    {"incognito-brand-consistency-for-ios",
     flag_descriptions::kIncognitoBrandConsistencyForIOSName,
     flag_descriptions::kIncognitoBrandConsistencyForIOSDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIncognitoBrandConsistencyForIOS)},
    {"update-history-entry-points-in-incognito",
     flag_descriptions::kUpdateHistoryEntryPointsInIncognitoName,
     flag_descriptions::kUpdateHistoryEntryPointsInIncognitoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUpdateHistoryEntryPointsInIncognito)},
};

bool SkipConditionalFeatureEntry(const flags_ui::FeatureEntry& entry) {
  return false;
}

flags_ui::FlagsState& GetGlobalFlagsState() {
  static base::NoDestructor<flags_ui::FlagsState> flags_state(kFeatureEntries,
                                                              nullptr);
  return *flags_state;
}
}  // namespace

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
                                    web::BuildMobileUserAgent(product));
  }

  // Shared variables for all enterprise experimental flags.
  NSMutableDictionary* testing_policies = [[NSMutableDictionary alloc] init];
  NSMutableArray* allowed_experimental_policies = [[NSMutableArray alloc] init];

  // Set some sample policy values for testing if EnableSamplePolicies is set to
  // true.
  if ([defaults boolForKey:@"EnableSamplePolicies"]) {
    // Define sample policies to enable. If some of the sample policies are
    // still marked as experimental (future_on), they must be explicitly
    // allowed, otherwise they will be ignored in Beta and Stable. Add them to
    // the |allowed_experimental_policies| array.
    [allowed_experimental_policies addObjectsFromArray:@[
      base::SysUTF8ToNSString(policy::key::kNTPContentSuggestionsEnabled),
    ]];

    [testing_policies addEntriesFromDictionary:@{
      base::SysUTF8ToNSString(policy::key::kAutofillAddressEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kAutofillCreditCardEnabled) : @NO,

      // 2 = Disable all variations
      base::SysUTF8ToNSString(policy::key::kChromeVariations) : @2,

      // 2 = Do not allow any site to show popups
      base::SysUTF8ToNSString(policy::key::kDefaultPopupsSetting) : @2,

      // Set default search engine.
      base::SysUTF8ToNSString(policy::key::kDefaultSearchProviderEnabled) :
          @YES,
      base::SysUTF8ToNSString(policy::key::kDefaultSearchProviderSearchURL) :
          @"http://www.google.com/search?q={searchTerms}",
      base::SysUTF8ToNSString(policy::key::kDefaultSearchProviderName) :
          @"TestEngine",

      base::SysUTF8ToNSString(policy::key::kEditBookmarksEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kNTPContentSuggestionsEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kPasswordManagerEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kTranslateEnabled) : @NO,

      // 2 = Enhanced safe browsing protection
      base::SysUTF8ToNSString(policy::key::kSafeBrowsingProtectionLevel) : @2,

      base::SysUTF8ToNSString(policy::key::kSearchSuggestEnabled) : @YES,

      // 0 = browser sign-in disabled
      base::SysUTF8ToNSString(policy::key::kBrowserSignin) : @0,
    }];
  }

  if ([defaults boolForKey:@"EnableSyncDisabledPolicy"]) {
    [testing_policies addEntriesFromDictionary:@{
      base::SysUTF8ToNSString(policy::key::kSyncDisabled) : @YES
    }];
    NSString* sync_policy_key =
        base::SysUTF8ToNSString(policy::key::kSyncDisabled);
    [allowed_experimental_policies addObject:sync_policy_key];
  }

  // If an incognito mode availability is set, add the policy key to the list of
  // allowed experimental policies, and set the value.
  NSString* incognito_policy_key =
      base::SysUTF8ToNSString(policy::key::kIncognitoModeAvailability);
  NSInteger incognito_mode_availability =
      [defaults integerForKey:incognito_policy_key];
  if (incognito_mode_availability) {
    [allowed_experimental_policies addObject:incognito_policy_key];
    [testing_policies addEntriesFromDictionary:@{
      incognito_policy_key : @(incognito_mode_availability),
    }];
  }

  // If a CBCM enrollment token is provided, force Chrome Browser Cloud
  // Management to enabled and add the token to the list of policies.
  NSString* token_key =
      base::SysUTF8ToNSString(policy::key::kCloudManagementEnrollmentToken);
  NSString* token = [defaults stringForKey:token_key];

  if ([token length] > 0) {
    command_line->AppendSwitch(switches::kEnableChromeBrowserCloudManagement);
    [testing_policies setValue:token forKey:token_key];
  }

  // If any experimental policy was allowed, set the EnableExperimentalPolicies
  // policy.
  if ([allowed_experimental_policies count] > 0) {
    [testing_policies setValue:allowed_experimental_policies
                        forKey:base::SysUTF8ToNSString(
                                   policy::key::kEnableExperimentalPolicies)];
  }

  // If some policies were set, commit them to the app's registration defaults.
  if ([testing_policies count] > 0) {
    NSDictionary* registration_defaults =
        @{kPolicyLoaderIOSConfigurationKey : testing_policies};
    [defaults registerDefaults:registration_defaults];
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
        // iOS keyboard replaces -- with —, so undo that.
        flag = [flag stringByReplacingOccurrencesOfString:@"—"
                                               withString:@"--"
                                                  options:0
                                                    range:NSMakeRange(0, 1)];
        // To make things easier, allow flags with no dashes by prepending them
        // here. This also allows for flags that just have one dash if they
        // exist.
        if (![flag hasPrefix:@"-"]) {
          flag = [@"--" stringByAppendingString:flag];
        }
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

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line) {
  GetGlobalFlagsState().ConvertFlagsToSwitches(
      flags_storage, command_line, flags_ui::kAddSentinels,
      switches::kEnableFeatures, switches::kDisableFeatures);
}

std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list) {
  // Occasionally DCHECK crashes on canary can be very distuptive.  An
  // experimental flag was added to aid in temporarily disabling this for
  // canary testers.
#if defined(DCHECK_IS_CONFIGURABLE)
  if (experimental_flags::AreDCHECKCrashesDisabled()) {
    std::vector<base::FeatureList::FeatureOverrideInfo> overrides;
    overrides.push_back(
        {std::cref(base::kDCheckIsFatalFeature),
         base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE});
    feature_list->RegisterExtraFeatureOverrides(std::move(overrides));
  }
#endif  // defined(DCHECK_IS_CONFIGURABLE)

  return GetGlobalFlagsState().RegisterAllFeatureVariationParameters(
      flags_storage, feature_list);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries) {
  GetGlobalFlagsState().GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::BindRepeating(&SkipConditionalFeatureEntry));
}

void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable) {
  GetGlobalFlagsState().SetFeatureEntryEnabled(flags_storage, internal_name,
                                               enable);
}

void ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
  GetGlobalFlagsState().ResetAllFlags(flags_storage);
}

namespace testing {

const flags_ui::FeatureEntry* GetFeatureEntries(size_t* count) {
  *count = base::size(kFeatureEntries);
  return kFeatureEntries;
}

}  // namespace testing
