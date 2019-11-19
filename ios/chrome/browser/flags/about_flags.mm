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
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
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
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#include "components/search_provider_logos/switches.h"
#include "components/security_state/core/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/ios/browser/features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/ukm/ios/features.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/features.h"
#include "ios/chrome/browser/crash_report/crash_report_flags.h"
#include "ios/chrome/browser/drag_and_drop/drag_and_drop_flag.h"
#include "ios/chrome/browser/find_in_page/features.h"
#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#include "ios/chrome/browser/passwords/password_manager_features.h"
#include "ios/chrome/browser/reading_list/features.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/dialogs/dialog_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#include "ios/chrome/browser/ui/settings/autofill/features.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_features.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/web/features.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/common/features.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/common/web_view_creation_util.h"

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
const FeatureEntry::FeatureParam kMarkHttpAsWarningAndDangerousOnFormEdits[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::
         kMarkHttpAsParameterWarningAndDangerousOnFormEdits}};
const FeatureEntry::FeatureParam kMarkHttpAsDangerWarning[] = {
    {security_state::features::kMarkHttpAsFeatureParameterName,
     security_state::features::kMarkHttpAsParameterDangerWarning}};

const FeatureEntry::FeatureVariation kMarkHttpAsFeatureVariations[] = {
    {"(mark as actively dangerous)", kMarkHttpAsDangerous,
     base::size(kMarkHttpAsDangerous), nullptr},
    {"(mark with a Not Secure warning and dangerous on form edits)",
     kMarkHttpAsWarningAndDangerousOnFormEdits,
     base::size(kMarkHttpAsWarningAndDangerousOnFormEdits), nullptr},
    {"(mark with a grey triangle icon)", kMarkHttpAsDangerWarning,
     base::size(kMarkHttpAsDangerWarning), nullptr}};

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

const FeatureEntry::FeatureParam kDetectMainThreadFreezeTimeout3s[] = {
    {crash_report::kDetectMainThreadFreezeParameterName,
     crash_report::kDetectMainThreadFreezeParameter3s}};
const FeatureEntry::FeatureParam kDetectMainThreadFreezeTimeout5s[] = {
    {crash_report::kDetectMainThreadFreezeParameterName,
     crash_report::kDetectMainThreadFreezeParameter5s}};
const FeatureEntry::FeatureParam kDetectMainThreadFreezeTimeout7s[] = {
    {crash_report::kDetectMainThreadFreezeParameterName,
     crash_report::kDetectMainThreadFreezeParameter7s}};
const FeatureEntry::FeatureParam kDetectMainThreadFreezeTimeout9s[] = {
    {crash_report::kDetectMainThreadFreezeParameterName,
     crash_report::kDetectMainThreadFreezeParameter9s}};

const FeatureEntry::FeatureVariation kDetectMainThreadFreezeVariations[] = {
    {"3s", kDetectMainThreadFreezeTimeout3s,
     base::size(kDetectMainThreadFreezeTimeout3s), nullptr},
    {"5s", kDetectMainThreadFreezeTimeout5s,
     base::size(kDetectMainThreadFreezeTimeout5s), nullptr},
    {"7s", kDetectMainThreadFreezeTimeout7s,
     base::size(kDetectMainThreadFreezeTimeout7s), nullptr},
    {"9s", kDetectMainThreadFreezeTimeout9s,
     base::size(kDetectMainThreadFreezeTimeout9s), nullptr}};

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
    {"drag_and_drop", flag_descriptions::kDragAndDropName,
     flag_descriptions::kDragAndDropDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDragAndDrop)},
    {"ignores-viewport-scale-limits",
     flag_descriptions::kIgnoresViewportScaleLimitsName,
     flag_descriptions::kIgnoresViewportScaleLimitsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIgnoresViewportScaleLimits)},
    {"slim-navigation-manager", flag_descriptions::kSlimNavigationManagerName,
     flag_descriptions::kSlimNavigationManagerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kSlimNavigationManager)},
    {"enable-autofill-credit-card-upload",
     flag_descriptions::kAutofillCreditCardUploadName,
     flag_descriptions::kAutofillCreditCardUploadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUpstream)},
    {"enable-sync-device-info-in-transport-mode",
     flag_descriptions::kSyncDeviceInfoInTransportModeName,
     flag_descriptions::kSyncDeviceInfoInTransportModeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kSyncDeviceInfoInTransportMode)},
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
    {"new-clear-browsing-data-ui",
     flag_descriptions::kNewClearBrowsingDataUIName,
     flag_descriptions::kNewClearBrowsingDataUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewClearBrowsingDataUI)},
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
    {"autofill-rich-metadata-queries",
     flag_descriptions::kAutofillRichMetadataQueriesName,
     flag_descriptions::kAutofillRichMetadataQueriesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillRichMetadataQueries)},
    {"fullscreen-viewport-adjustment-experiment",
     flag_descriptions::kFullscreenSmoothScrollingName,
     flag_descriptions::kFullscreenSmoothScrollingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(fullscreen::features::kSmoothScrollingDefault)},
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
    {"autofill-cache-query-responses",
     flag_descriptions::kAutofillCacheQueryResponsesName,
     flag_descriptions::kAutofillCacheQueryResponsesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillCacheQueryResponses)},
    {"autofill-enable-company-name",
     flag_descriptions::kAutofillEnableCompanyNameName,
     flag_descriptions::kAutofillEnableCompanyNameDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCompanyName)},
    {"webpage-text-accessibility",
     flag_descriptions::kWebPageTextAccessibilityName,
     flag_descriptions::kWebPageTextAccessibilityDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageTextAccessibility)},
    {"toolbar-container", flag_descriptions::kToolbarContainerName,
     flag_descriptions::kToolbarContainerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(toolbar_container::kToolbarContainerEnabled)},
    {"omnibox-popup-shortcuts",
     flag_descriptions::kOmniboxPopupShortcutIconsInZeroStateName,
     flag_descriptions::kOmniboxPopupShortcutIconsInZeroStateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPopupShortcutIconsInZeroState)},
    {"omnibox-on-device-head-suggestions",
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsName,
     flag_descriptions::kOmniboxOnDeviceHeadSuggestionsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kOnDeviceHeadProvider)},
    {"omnibox-ui-max-autocomplete-matches",
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesName,
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kUIExperimentMaxAutocompleteMatches,
         kOmniboxUIMaxAutocompleteMatchesVariations,
         "OmniboxUIMaxAutocompleteVariations")},
    {"search-icon-toggle", flag_descriptions::kSearchIconToggleName,
     flag_descriptions::kSearchIconToggleDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIconForSearchButtonFeature,
                                    kIconForSearchButtonVariations,
                                    "ToggleSearchButtonIcon")},
    {"enable-breakpad-upload-no-delay",
     flag_descriptions::kBreakpadNoDelayInitialUploadName,
     flag_descriptions::kBreakpadNoDelayInitialUploadDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(crash_report::kBreakpadNoDelayInitialUpload)},
    {"non-modal-dialogs", flag_descriptions::kNonModalDialogsName,
     flag_descriptions::kNonModalDialogsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(dialogs::kNonModalDialogs)},
    {"detect-main-thread-freeze",
     flag_descriptions::kDetectMainThreadFreezeName,
     flag_descriptions::kDetectMainThreadFreezeDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(crash_report::kDetectMainThreadFreeze,
                                    kDetectMainThreadFreezeVariations,
                                    "DetectMainThreadFreeze")},
    {"infobar-ui-reboot", flag_descriptions::kInfobarUIRebootName,
     flag_descriptions::kInfobarUIRebootDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kInfobarUIReboot)},
    {"find-in-page-iframe", flag_descriptions::kFindInPageiFrameName,
     flag_descriptions::kFindInPageiFrameDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFindInPageiFrame)},
    {"enable-clipboard-provider-text-suggestions",
     flag_descriptions::kEnableClipboardProviderTextSuggestionsName,
     flag_descriptions::kEnableClipboardProviderTextSuggestionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kEnableClipboardProviderTextSuggestions)},
    {"enable-clipboard-provider-image-suggestions",
     flag_descriptions::kEnableClipboardProviderImageSuggestionsName,
     flag_descriptions::kEnableClipboardProviderImageSuggestionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kEnableClipboardProviderImageSuggestions)},
    {"snapshot-draw-view", flag_descriptions::kSnapshotDrawViewName,
     flag_descriptions::kSnapshotDrawViewDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSnapshotDrawView)},
#if defined(DCHECK_IS_CONFIGURABLE)
    {"dcheck-is-fatal", flag_descriptions::kDcheckIsFatalName,
     flag_descriptions::kDcheckIsFatalDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(base::kDCheckIsFatalFeature)},
#endif  // defined(DCHECK_IS_CONFIGURABLE)
    {"settings-refresh", flag_descriptions::kSettingsRefreshName,
     flag_descriptions::kSettingsRefreshDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSettingsRefresh)},
    {"browser-container-keeps-content-view",
     flag_descriptions::kBrowserContainerKeepsContentViewName,
     flag_descriptions::kBrowserContainerKeepsContentViewDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kBrowserContainerKeepsContentView)},
    {"web-clear-browsing-data", flag_descriptions::kWebClearBrowsingDataName,
     flag_descriptions::kWebClearBrowsingDataDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kWebClearBrowsingData)},
    {"send-uma-cellular", flag_descriptions::kSendUmaOverAnyNetwork,
     flag_descriptions::kSendUmaOverAnyNetworkDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUmaCellular)},
    {"enable-sync-uss-passwords",
     flag_descriptions::kEnableSyncUSSPasswordsName,
     flag_descriptions::kEnableSyncUSSPasswordsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kSyncUSSPasswords)},
    {"offline-page-without-native-content",
     flag_descriptions::kOfflineVersionWithoutNativeContentName,
     flag_descriptions::kOfflineVersionWithoutNativeContentDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(reading_list::kOfflineVersionWithoutNativeContent)},
    {"autofill-no-local-save-on-upload-success",
     flag_descriptions::kAutofillNoLocalSaveOnUploadSuccessName,
     flag_descriptions::kAutofillNoLocalSaveOnUploadSuccessDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillNoLocalSaveOnUploadSuccess)},
    {"autofill-no-local-save-on-unmask-success",
     flag_descriptions::kAutofillNoLocalSaveOnUnmaskSuccessName,
     flag_descriptions::kAutofillNoLocalSaveOnUnmaskSuccessDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillNoLocalSaveOnUnmaskSuccess)},
    {"new-omnibox-popup-layout", flag_descriptions::kNewOmniboxPopupLayoutName,
     flag_descriptions::kNewOmniboxPopupLayoutDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewOmniboxPopupLayout)},
    {"omnibox-use-default-search-engine-favicon",
     flag_descriptions::kOmniboxUseDefaultSearchEngineFaviconName,
     flag_descriptions::kOmniboxUseDefaultSearchEngineFaviconDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOmniboxUseDefaultSearchEngineFavicon)},
    {"enable-send-tab-to-self-broadcast",
     flag_descriptions::kSendTabToSelfBroadcastName,
     flag_descriptions::kSendTabToSelfBroadcastDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfBroadcast)},
    {"autofill-use-mobile-label-disambiguation",
     flag_descriptions::kAutofillUseMobileLabelDisambiguationName,
     flag_descriptions::kAutofillUseMobileLabelDisambiguationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUseMobileLabelDisambiguation,
         kAutofillUseMobileLabelDisambiguationVariations,
         "AutofillUseMobileLabelDisambiguation")},
    {"enable-autofill-prune-suggestions",
     flag_descriptions::kAutofillPruneSuggestionsName,
     flag_descriptions::kAutofillPruneSuggestionsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPruneSuggestions)},
    {"language-settings", flag_descriptions::kLanguageSettingsName,
     flag_descriptions::kLanguageSettingsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLanguageSettings)},
    {"toolbar-new-tab-button", flag_descriptions::kToolbarNewTabButtonName,
     flag_descriptions::kToolbarNewTabButtonDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kToolbarNewTabButton)},
    {"enable-sync-uss-nigori", flag_descriptions::kEnableSyncUSSNigoriName,
     flag_descriptions::kEnableSyncUSSNigoriDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kSyncUSSNigori)},
    {"settings-add-payment-method",
     flag_descriptions::kSettingsAddPaymentMethodName,
     flag_descriptions::kSettingsAddPaymentMethodDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSettingsAddPaymentMethod)},
    {"collections-card-presentation-style",
     flag_descriptions::kCollectionsCardPresentationStyleName,
     flag_descriptions::kCollectionsCardPresentationStyleDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCollectionsCardPresentationStyle)},
    {"enable-autofill-credit-card-upload-editable-cardholder-name",
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableCardholderNameName,
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableCardholderNameDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamEditableCardholderName)},
    {"enable-autofill-credit-card-upload-editable-expiration-date",
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableExpirationDateName,
     flag_descriptions::
         kEnableAutofillCreditCardUploadEditableExpirationDateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamEditableExpirationDate)},
    {"credit-card-scanner", flag_descriptions::kCreditCardScannerName,
     flag_descriptions::kCreditCardScannerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCreditCardScanner)},
    {"ios-breadcrumbs", flag_descriptions::kLogBreadcrumbsName,
     flag_descriptions::kLogBreadcrumbsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLogBreadcrumbs)},
    {"password-leak-detection", flag_descriptions::kPasswordLeakDetectionName,
     flag_descriptions::kPasswordLeakDetectionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kLeakDetection)},
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(signin::kForceStartupSigninPromo)},
    {"embedder-block-restore-url",
     flag_descriptions::kEmbedderBlockRestoreUrlName,
     flag_descriptions::kEmbedderBlockRestoreUrlDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEmbedderBlockRestoreUrl)},
    {"messages-confirm-infobars",
     flag_descriptions::kConfirmInfobarMessagesUIName,
     flag_descriptions::kConfirmInfobarMessagesUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kConfirmInfobarMessagesUI)},
    {"disable-animation-on-low-battery",
     flag_descriptions::kDisableAnimationOnLowBatteryName,
     flag_descriptions::kDisableAnimationOnLowBatteryDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDisableAnimationOnLowBattery)},
    {"messages-save-card-infobar",
     flag_descriptions::kSaveCardInfobarMessagesUIName,
     flag_descriptions::kSaveCardInfobarMessagesUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSaveCardInfobarMessagesUI)},
    {"messages-translate-infobar",
     flag_descriptions::kTranslateInfobarMessagesUIName,
     flag_descriptions::kTranslateInfobarMessagesUIDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kTranslateInfobarMessagesUI)},
    {"use-WKWebView-loading", flag_descriptions::kUseWKWebViewLoadingName,
     flag_descriptions::kUseWKWebViewLoadingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kUseWKWebViewLoading)},
    {"autofill-save-card-dismiss-on-navigation",
     flag_descriptions::kAutofillSaveCardDismissOnNavigationName,
     flag_descriptions::kAutofillSaveCardDismissOnNavigationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSaveCardDismissOnNavigation)},
    {"enable-persistent-downloads",
     flag_descriptions::kEnablePersistentDownloadsName,
     flag_descriptions::kEnablePersistentDownloadsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnablePersistentDownloads)},
    {"force-unstacked-tabstrip", flag_descriptions::kForceUnstackedTabstripName,
     flag_descriptions::kForceUnstackedTabstripDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kForceUnstackedTabstrip)},
    {"use-js-error-page", flag_descriptions::kUseJSForErrorPageName,
     flag_descriptions::kUseJSForErrorPageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kUseJSForErrorPage)},
    {"messages-download-infobar",
     flag_descriptions::kDownloadInfobarMessagesUIName,
     flag_descriptions::kDownloadInfobarMessagesUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDownloadInfobarMessagesUI)},
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

bool SkipConditionalFeatureEntry(const flags_ui::FeatureEntry& entry) {
  return false;
}

flags_ui::FlagsState& GetGlobalFlagsState() {
  static base::NoDestructor<flags_ui::FlagsState> flags_state(kFeatureEntries,
                                                              nullptr);
  return *flags_state;
}
}  // namespace

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line) {
  AppendSwitchesFromExperimentalSettings(command_line);
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
      base::Bind(&SkipConditionalFeatureEntry));
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
