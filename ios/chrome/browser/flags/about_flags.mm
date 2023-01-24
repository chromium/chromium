// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of about_flags for iOS that sets flags based on experimental
// settings.

#import "ios/chrome/browser/flags/about_flags.h"

#import <UIKit/UIKit.h>
#import <stddef.h>
#import <stdint.h>

#import "base/base_switches.h"
#import "base/bind.h"
#import "base/callback_helpers.h"
#import "base/check_op.h"
#import "base/debug/debugging_buildflags.h"
#import "base/mac/foundation_util.h"
#import "base/no_destructor.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/core/common/autofill_switches.h"
#import "components/autofill/ios/browser/autofill_switches.h"
#import "components/breadcrumbs/core/features.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/flag_descriptions.h"
#import "components/content_settings/core/common/features.h"
#import "components/dom_distiller/core/dom_distiller_switches.h"
#import "components/download/public/background_service/features.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feed/feed_feature_list.h"
#import "components/flags_ui/feature_entry.h"
#import "components/flags_ui/feature_entry_macros.h"
#import "components/flags_ui/flags_storage.h"
#import "components/flags_ui/flags_ui_switches.h"
#import "components/invalidation/impl/invalidation_switches.h"
#import "components/ntp_tiles/features.h"
#import "components/ntp_tiles/switches.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/payments/core/features.h"
#import "components/policy/core/common/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/send_tab_to_self/features.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "components/signin/core/browser/account_reconcilor.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/features.h"
#import "components/sync/base/pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/browsing_data/browsing_data_features.h"
#import "ios/chrome/browser/crash_report/features.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/policy/cloud/user_policy_constants.h"
#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/screen_time/screen_time_buildflags.h"
#import "ios/chrome/browser/sessions/session_features.h"
#import "ios/chrome/browser/text_selection/text_selection_util.h"
#import "ios/chrome/browser/ui/app_store_rating/features.h"
#import "ios/chrome/browser/ui/autofill/features.h"
#import "ios/chrome/browser/ui/bubble/bubble_features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/download/features.h"
#import "ios/chrome/browser/ui/first_run/field_trial_constants.h"
#import "ios/chrome/browser/ui/first_run/trending_queries_field_trial.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/keyboard/features.h"
#import "ios/chrome/browser/ui/ntp/field_trial_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/open_in/features.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/features.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_features.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/whats_new/feature_flags.h"
#import "ios/chrome/browser/web/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"
#import "ios/web/common/features.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/common/web_view_creation_util.h"

#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
#import "ios/chrome/browser/screen_time/features.h"
#endif

#if !defined(OFFICIAL_BUILD)
#import "components/variations/variations_switches.h"
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

const FeatureEntry::Choice
    kWaitThresholdMillisecondsForCapabilitiesApiChoices[] = {
        {flags_ui::kGenericExperimentChoiceDefault, "", ""},
        {"200", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "200"},
        {"500", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "500"},
        {"5000", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "5000"},
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
         std::size(kOmniboxUIMaxAutocompleteMatches3), nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4,
         std::size(kOmniboxUIMaxAutocompleteMatches4), nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5,
         std::size(kOmniboxUIMaxAutocompleteMatches5), nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6,
         std::size(kOmniboxUIMaxAutocompleteMatches6), nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8,
         std::size(kOmniboxUIMaxAutocompleteMatches8), nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10,
         std::size(kOmniboxUIMaxAutocompleteMatches10), nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12,
         std::size(kOmniboxUIMaxAutocompleteMatches12), nullptr}};

const FeatureEntry::FeatureParam kOmniboxMaxZPSMatches6[] = {
    {OmniboxFieldTrial::kMaxZeroSuggestMatchesParam, "6"}};
const FeatureEntry::FeatureParam kOmniboxMaxZPSMatches15[] = {
    {OmniboxFieldTrial::kMaxZeroSuggestMatchesParam, "15"}};
const FeatureEntry::FeatureParam kOmniboxMaxZPSMatches20[] = {
    {OmniboxFieldTrial::kMaxZeroSuggestMatchesParam, "20"}};

const FeatureEntry::FeatureVariation kOmniboxMaxZPSMatchesVariations[] = {
    {"6 matches", kOmniboxMaxZPSMatches6, std::size(kOmniboxMaxZPSMatches6),
     nullptr},
    {"15 matches", kOmniboxMaxZPSMatches15, std::size(kOmniboxMaxZPSMatches15),
     nullptr},
    {"20 matches", kOmniboxMaxZPSMatches20, std::size(kOmniboxMaxZPSMatches20),
     nullptr},
};

const FeatureEntry::FeatureParam kOmniboxMaxURLMatches5[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "5"}};
const FeatureEntry::FeatureParam kOmniboxMaxURLMatches6[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "6"}};
const FeatureEntry::FeatureParam kOmniboxMaxURLMatches7[] = {
    {OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "7"}};

const FeatureEntry::FeatureVariation kOmniboxMaxURLMatchesVariations[] = {
    {"5 matches", kOmniboxMaxURLMatches5, std::size(kOmniboxMaxURLMatches5),
     nullptr},
    {"6 matches", kOmniboxMaxURLMatches6, std::size(kOmniboxMaxURLMatches6),
     nullptr},
    {"7 matches", kOmniboxMaxURLMatches7, std::size(kOmniboxMaxURLMatches7),
     nullptr},
};

const FeatureEntry::FeatureParam kWhatsNewModuleLayout[] = {
    {kWhatsNewModuleBasedLayoutParam, "true"}};

const FeatureEntry::FeatureVariation kWhatsNewLayoutVariations[] = {
    {"Display module layout", kWhatsNewModuleLayout,
     std::size(kWhatsNewModuleLayout), nullptr},
};

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
         std::size(kAutofillUseMobileLabelDisambiguationShowAll), nullptr},
        {"(show one)", kAutofillUseMobileLabelDisambiguationShowOne,
         std::size(kAutofillUseMobileLabelDisambiguationShowOne), nullptr}};

const FeatureEntry::FeatureParam
    kDefaultBrowserFullscreenPromoExperimentRemindMeLater[] = {
        {kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam, "true"}};
const FeatureEntry::FeatureVariation
    kDefaultBrowserFullscreenPromoExperimentVariations[] = {
        {"Remind me later",
         kDefaultBrowserFullscreenPromoExperimentRemindMeLater,
         std::size(kDefaultBrowserFullscreenPromoExperimentRemindMeLater),
         nullptr}};

const FeatureEntry::FeatureParam kDiscoverFeedTopSyncPromoFullWithTitle[] = {
    {kDiscoverFeedTopSyncPromoStyleFullWithTitle, "true"},
    {kDiscoverFeedTopSyncPromoStyleCompact, "false"}};
const FeatureEntry::FeatureParam kDiscoverFeedTopSyncPromoCompact[] = {
    {kDiscoverFeedTopSyncPromoStyleFullWithTitle, "false"},
    {kDiscoverFeedTopSyncPromoStyleCompact, "true"}};

const FeatureEntry::FeatureVariation kDiscoverFeedTopSyncPromoVariations[] = {
    {"Full with title", kDiscoverFeedTopSyncPromoFullWithTitle,
     std::size(kDiscoverFeedTopSyncPromoFullWithTitle), nullptr},
    {"Compact", kDiscoverFeedTopSyncPromoCompact,
     std::size(kDiscoverFeedTopSyncPromoCompact), nullptr}};

const FeatureEntry::FeatureParam kiOSOmniboxUpdatedPopupUIVersion1[] = {
    {kIOSOmniboxUpdatedPopupUIVariationName,
     kIOSOmniboxUpdatedPopupUIVariation1}};
const FeatureEntry::FeatureParam kiOSOmniboxUpdatedPopupUIVersion2[] = {
    {kIOSOmniboxUpdatedPopupUIVariationName,
     kIOSOmniboxUpdatedPopupUIVariation2}};
const FeatureEntry::FeatureParam kiOSOmniboxUpdatedPopupUIVersion3[] = {
    {kIOSOmniboxUpdatedPopupUIVariationName,
     kIOSOmniboxUpdatedPopupUIVariation1UIKit}};
const FeatureEntry::FeatureParam kiOSOmniboxUpdatedPopupUIVersion4[] = {
    {kIOSOmniboxUpdatedPopupUIVariationName,
     kIOSOmniboxUpdatedPopupUIVariation2UIKit}};
const FeatureEntry::FeatureVariation kiOSOmniboxUpdatedPopupUIVariations[] = {
    {"Version 1 - SwiftUI", kiOSOmniboxUpdatedPopupUIVersion1,
     std::size(kiOSOmniboxUpdatedPopupUIVersion1), nullptr},
    {"Version 2 - SwiftUI", kiOSOmniboxUpdatedPopupUIVersion2,
     std::size(kiOSOmniboxUpdatedPopupUIVersion2), nullptr},
    {"Version 1 - UIKit", kiOSOmniboxUpdatedPopupUIVersion3,
     std::size(kiOSOmniboxUpdatedPopupUIVersion3), nullptr},
    {"Version 2 - UIKit", kiOSOmniboxUpdatedPopupUIVersion4,
     std::size(kiOSOmniboxUpdatedPopupUIVersion4), nullptr}};

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
     std::size(kStartSurfaceTenSecondsReturnToRecentTab), nullptr},
    {"10s:Shrink Logo", kStartSurfaceTenSecondsShrinkLogo,
     std::size(kStartSurfaceTenSecondsShrinkLogo), nullptr},
    {"10s:Hide Shortcuts", kStartSurfaceTenSecondsHideShortcuts,
     std::size(kStartSurfaceTenSecondsHideShortcuts), nullptr},
    {"10s:Shrink Logo and show Return to Recent Tab tile",
     kStartSurfaceTenSecondsShrinkLogoReturnToRecentTab,
     std::size(kStartSurfaceTenSecondsShrinkLogoReturnToRecentTab), nullptr},
    {"10s:Hide Shortcuts and show Return to Recent Tab tile",
     kStartSurfaceTenSecondsHideShortcutsReturnToRecentTab,
     std::size(kStartSurfaceTenSecondsHideShortcutsReturnToRecentTab), nullptr},
    {"1h:Show Return to Recent Tab tile", kStartSurfaceOneHourReturnToRecentTab,
     std::size(kStartSurfaceOneHourReturnToRecentTab), nullptr},
    {"1h:Shrink Logo", kStartSurfaceOneHourShrinkLogo,
     std::size(kStartSurfaceOneHourShrinkLogo), nullptr},
    {"1h:Hide Shortcuts", kStartSurfaceOneHourHideShortcuts,
     std::size(kStartSurfaceOneHourHideShortcuts), nullptr},
    {"1h:Shrink Logo and show Return to Recent Tab tile",
     kStartSurfaceOneHourShrinkLogoReturnToRecentTab,
     std::size(kStartSurfaceOneHourShrinkLogoReturnToRecentTab), nullptr},
    {"1h:Hide Shortcuts and show Return to Recent Tab tile",
     kStartSurfaceOneHourHideShortcutsReturnToRecentTab,
     std::size(kStartSurfaceOneHourHideShortcutsReturnToRecentTab), nullptr},
};

const FeatureEntry::FeatureParam kModuleRefreshMinimizeSpacing[] = {
    {kContentSuggestionsUIModuleRefreshMinimizeSpacingParam, "true"}};
const FeatureEntry::FeatureParam kModuleRefreshNoHeaders[] = {
    {kContentSuggestionsUIModuleRefreshMinimizeSpacingParam, "true"},
    {kContentSuggestionsUIModuleRefreshRemoveHeadersParam, "true"}};

const FeatureEntry::FeatureVariation kModuleRefreshVariations[] = {
    {"Enabled with minimized spacing", kModuleRefreshMinimizeSpacing,
     std::size(kModuleRefreshMinimizeSpacing), nullptr},
    {"Enabled with no headers and minimized spacing", kModuleRefreshNoHeaders,
     std::size(kModuleRefreshNoHeaders), nullptr},
};

#if BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
// Feed Background Refresh Feature Params
const FeatureEntry::FeatureParam kOneHourIntervalOneHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "false"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, /* 60*60= */ "3600"}};
const FeatureEntry::FeatureParam kFourHourIntervalSixHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "false"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, /* 4*60*60= */ "14400"}};
const FeatureEntry::FeatureParam kOneHourIntervalOneHourMaxAgeRecurring[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "false"},
    {kEnableRecurringBackgroundRefreshSchedule, "true"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, /* 60*60= */ "3600"}};
const FeatureEntry::FeatureParam kFourHourIntervalSixHourMaxAgeRecurring[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "false"},
    {kEnableRecurringBackgroundRefreshSchedule, "true"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, /* 4*60*60= */ "14400"}};
const FeatureEntry::FeatureParam kServerDrivenOneHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};
const FeatureEntry::FeatureParam kServerDrivenOneHourMaxAgeRecurring[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "true"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};
const FeatureEntry::FeatureParam kServerDrivenSixHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};
const FeatureEntry::FeatureParam kServerDrivenSixHourMaxAgeRecurring[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "true"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};

// Feed Background Refresh Feature Variations
const FeatureEntry::FeatureVariation kFeedBackgroundRefreshVariations[] = {
    {"1hr Interval 1hr Max Age Once", kOneHourIntervalOneHourMaxAgeOnce,
     std::size(kOneHourIntervalOneHourMaxAgeOnce), nullptr},
    {"4hr Interval 6hr Max Age Once", kFourHourIntervalSixHourMaxAgeOnce,
     std::size(kFourHourIntervalSixHourMaxAgeOnce), nullptr},
    {"1hr Interval 1hr Max Age Recurring",
     kOneHourIntervalOneHourMaxAgeRecurring,
     std::size(kOneHourIntervalOneHourMaxAgeRecurring), nullptr},
    {"4hr Interval 6hr Max Age Recurring",
     kFourHourIntervalSixHourMaxAgeRecurring,
     std::size(kFourHourIntervalSixHourMaxAgeRecurring), nullptr},
    {"Server Driven 1hr Max Age Once", kServerDrivenOneHourMaxAgeOnce,
     std::size(kServerDrivenOneHourMaxAgeOnce), nullptr},
    {"Server Driven 1hr Max Age Recurring", kServerDrivenOneHourMaxAgeRecurring,
     std::size(kServerDrivenOneHourMaxAgeRecurring), nullptr},
    {"Server Driven 6hr Max Age Once", kServerDrivenSixHourMaxAgeOnce,
     std::size(kServerDrivenSixHourMaxAgeOnce), nullptr},
    {"Server Driven 6hr Max Age Recurring", kServerDrivenSixHourMaxAgeRecurring,
     std::size(kServerDrivenSixHourMaxAgeRecurring), nullptr},
};
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)

const FeatureEntry::FeatureParam kFREDefaultBrowserPromoDefaultDelay[] = {
    {kFREDefaultBrowserPromoParam, kFREDefaultBrowserPromoDefaultDelayParam}};
const FeatureEntry::FeatureParam kFREDefaultBrowserPromoFirstRunOnly[] = {
    {kFREDefaultBrowserPromoParam, kFREDefaultBrowserPromoFirstRunOnlyParam}};
const FeatureEntry::FeatureParam kFREDefaultBrowserPromoShortDelay[] = {
    {kFREDefaultBrowserPromoParam, kFREDefaultBrowserPromoShortDelayParam}};
const FeatureEntry::FeatureVariation kFREDefaultBrowserPromoVariations[] = {
    {"Wait 14 days after FRE default browser promo",
     kFREDefaultBrowserPromoDefaultDelay,
     std::size(kFREDefaultBrowserPromoDefaultDelay), nullptr},
    {"FRE default browser promo only", kFREDefaultBrowserPromoFirstRunOnly,
     std::size(kFREDefaultBrowserPromoFirstRunOnly), nullptr},
    {"Wait 3 days after FRE default browser promo",
     kFREDefaultBrowserPromoShortDelay,
     std::size(kFREDefaultBrowserPromoShortDelay), nullptr},
};

const FeatureEntry::FeatureParam kTrendingQueriesEnableAllUsers[] = {
    {kTrendingQueriesHideShortcutsParam, "false"}};
const FeatureEntry::FeatureParam kTrendingQueriesEnableAllUsersHideShortcuts[] =
    {{kTrendingQueriesHideShortcutsParam, "true"}};
const FeatureEntry::FeatureParam kTrendingQueriesEnableFeedDisabled[] = {
    {kTrendingQueriesHideShortcutsParam, "false"},
    {kTrendingQueriesDisabledFeedParam, "true"},
};

const FeatureEntry::FeatureVariation kTrendingQueriesModuleVariations[] = {
    {"Enabled All Users", kTrendingQueriesEnableAllUsers,
     std::size(kTrendingQueriesEnableAllUsers), nullptr},
    {"Enabled All Users Hide Shortcuts",
     kTrendingQueriesEnableAllUsersHideShortcuts,
     std::size(kTrendingQueriesEnableAllUsersHideShortcuts), nullptr},
    {"Enabled Disabled Feed", kTrendingQueriesEnableFeedDisabled,
     std::size(kTrendingQueriesEnableFeedDisabled), nullptr},
};

const FeatureEntry::FeatureParam kNewMICEFREWithTangibleSyncA[] = {
    {kNewMobileIdentityConsistencyFREParam,
     kNewMobileIdentityConsistencyFREParamTangibleSyncA}};
const FeatureEntry::FeatureParam kNewMICEFREWithTangibleSyncB[] = {
    {kNewMobileIdentityConsistencyFREParam,
     kNewMobileIdentityConsistencyFREParamTangibleSyncB}};
const FeatureEntry::FeatureParam kNewMICEFREWithTangibleSyncC[] = {
    {kNewMobileIdentityConsistencyFREParam,
     kNewMobileIdentityConsistencyFREParamTangibleSyncC}};
const FeatureEntry::FeatureParam kNewMICEFREWithTwoSteps[] = {
    {kNewMobileIdentityConsistencyFREParam,
     kNewMobileIdentityConsistencyFREParamTwoSteps}};
const FeatureEntry::FeatureParam kNewMICEFREWithTangibleSyncD[] = {
    {kNewMobileIdentityConsistencyFREParam,
     kNewMobileIdentityConsistencyFREParamTangibleSyncD}};
const FeatureEntry::FeatureParam kNewMICEFREWithTangibleSyncE[] = {
    {kNewMobileIdentityConsistencyFREParam,
     kNewMobileIdentityConsistencyFREParamTangibleSyncE}};
const FeatureEntry::FeatureParam kNewMICEFREWithTangibleSyncF[] = {
    {kNewMobileIdentityConsistencyFREParam,
     kNewMobileIdentityConsistencyFREParamTangibleSyncF}};
const FeatureEntry::FeatureVariation
    kNewMobileIdentityConsistencyFREVariations[] = {
        {"new FRE with tangible sync A", kNewMICEFREWithTangibleSyncA,
         std::size(kNewMICEFREWithTangibleSyncA), nullptr},
        {"new FRE with tangible sync B", kNewMICEFREWithTangibleSyncB,
         std::size(kNewMICEFREWithTangibleSyncB), nullptr},
        {"new FRE with tangible sync C", kNewMICEFREWithTangibleSyncC,
         std::size(kNewMICEFREWithTangibleSyncC), nullptr},
        {"new FRE with tangible sync D", kNewMICEFREWithTangibleSyncD,
         std::size(kNewMICEFREWithTangibleSyncD), nullptr},
        {"new FRE with tangible sync E", kNewMICEFREWithTangibleSyncE,
         std::size(kNewMICEFREWithTangibleSyncE), nullptr},
        {"new FRE with tangible sync F", kNewMICEFREWithTangibleSyncF,
         std::size(kNewMICEFREWithTangibleSyncF), nullptr},
        {"new FRE with 2 steps", kNewMICEFREWithTwoSteps,
         std::size(kNewMICEFREWithTwoSteps), nullptr}};

const FeatureEntry::FeatureParam kBubbleRichIPHTargetHighlight[] = {
    {kBubbleRichIPHParameterName, kBubbleRichIPHParameterTargetHighlight}};
const FeatureEntry::FeatureParam kBubbleRichIPHExplicitDismissal[] = {
    {kBubbleRichIPHParameterName, kBubbleRichIPHParameterExplicitDismissal}};
const FeatureEntry::FeatureParam kBubbleRichIPHRich[] = {
    {kBubbleRichIPHParameterName, kBubbleRichIPHParameterRich}};
const FeatureEntry::FeatureParam kBubbleRichIPHRichWithSnooze[] = {
    {kBubbleRichIPHParameterName, kBubbleRichIPHParameterRichWithSnooze}};
const FeatureEntry::FeatureVariation kBubbleRichIPHVariations[] = {
    {"Target Highlight", kBubbleRichIPHTargetHighlight,
     std::size(kBubbleRichIPHTargetHighlight), nullptr},
    {"Explicit dismissal", kBubbleRichIPHExplicitDismissal,
     std::size(kBubbleRichIPHExplicitDismissal), nullptr},
    {"Dismissal and rich content", kBubbleRichIPHRich,
     std::size(kBubbleRichIPHRich), nullptr},
    {"Dismissal, rich content, and snooze", kBubbleRichIPHRichWithSnooze,
     std::size(kBubbleRichIPHRichWithSnooze), nullptr},
};

const FeatureEntry::FeatureParam kOmniboxPasteButtonBlueIconCapsule[] = {
    {kOmniboxPasteButtonParameterName,
     kOmniboxPasteButtonParameterBlueIconCapsule}};
const FeatureEntry::FeatureParam kOmniboxPasteButtonBlueFullCapsule[] = {
    {kOmniboxPasteButtonParameterName,
     kOmniboxPasteButtonParameterBlueFullCapsule}};
const FeatureEntry::FeatureVariation kOmniboxPasteButtonVariations[] = {
    {"Icon only", kOmniboxPasteButtonBlueIconCapsule,
     std::size(kOmniboxPasteButtonBlueIconCapsule), nullptr},
    {"Icon and text", kOmniboxPasteButtonBlueFullCapsule,
     std::size(kOmniboxPasteButtonBlueFullCapsule), nullptr},
};

const FeatureEntry::FeatureParam kDmTokenDeletionParam[] = {{"forced", "true"}};
const FeatureEntry::FeatureVariation kDmTokenDeletionVariation[] = {
    {"(Forced)", kDmTokenDeletionParam, std::size(kDmTokenDeletionParam),
     nullptr}};

const FeatureEntry::FeatureParam kOpenInDownloadInShareButton[] = {
    {kOpenInDownloadParameterName, kOpenInDownloadInShareButtonParam}};
const FeatureEntry::FeatureParam kOpenInDownloadWithWKDownload[] = {
    {kOpenInDownloadParameterName, kOpenInDownloadWithWKDownloadParam}};
const FeatureEntry::FeatureParam kOpenInDownloadWithV2[] = {
    {kOpenInDownloadParameterName, kOpenInDownloadWithV2Param}};

const FeatureEntry::FeatureVariation kOpenInDownloadVariations[] = {
    {"With legacy download", kOpenInDownloadInShareButton,
     std::size(kOpenInDownloadInShareButton), nullptr},
    {"With WKDownload", kOpenInDownloadWithWKDownload,
     std::size(kOpenInDownloadWithWKDownload), nullptr},
    {"With V2", kOpenInDownloadWithV2, std::size(kOpenInDownloadWithV2),
     nullptr},
};

const FeatureEntry::FeatureParam kEnablePinnedTabsBottomPosition[] = {
    {kEnablePinnedTabsParameterName, kEnablePinnedTabsBottomParam}};
const FeatureEntry::FeatureParam kEnablePinnedTabsTopPosition[] = {
    {kEnablePinnedTabsParameterName, kEnablePinnedTabsTopParam}};

const FeatureEntry::FeatureVariation kEnablePinnedTabsVariations[] = {
    {"bottom pinned tabs", kEnablePinnedTabsBottomPosition,
     std::size(kEnablePinnedTabsBottomPosition), nullptr},
    {"top pinned tabs", kEnablePinnedTabsTopPosition,
     std::size(kEnablePinnedTabsTopPosition), nullptr},
};

const FeatureEntry::FeatureParam kAutofillBrandingIOSMonotone[] = {
    {autofill::features::kAutofillBrandingIOSParam, "true"}};
const FeatureEntry::FeatureVariation kAutofillBrandingIOSVariations[] = {
    {"(Monotone)", kAutofillBrandingIOSMonotone,
     std::size(kAutofillBrandingIOSMonotone), nullptr}};

const FeatureEntry::FeatureParam kIOSNewPostRestoreExperienceMinimal[] = {
    {post_restore_signin::features::kIOSNewPostRestoreExperienceParam, "true"}};
const FeatureEntry::FeatureVariation kIOSNewPostRestoreExperienceVariations[] =
    {{"minimal", kIOSNewPostRestoreExperienceMinimal,
      std::size(kIOSNewPostRestoreExperienceMinimal), nullptr}};

const FeatureEntry::FeatureParam kIOSPopularSitesExcludePopularApps[] = {
    {ntp_tiles::kIOSPopularSitesExcludePopularAppsParam, "true"}};
const FeatureEntry::FeatureVariation
    kIOSPopularSitesImprovedSuggestionsVariations[] = {
        {"(Exclude popular apps)", kIOSPopularSitesExcludePopularApps,
         std::size(kIOSPopularSitesExcludePopularApps), nullptr}};

const FeatureEntry::FeatureParam kEnableExperienceKitMapsWithSrp[] = {
    {kExperienceKitMapsVariationName, kEnableExperienceKitMapsVariationSrp}};
const FeatureEntry::FeatureVariation kEnableExperienceKitMapsVariations[] = {
    {"with search result page", kEnableExperienceKitMapsWithSrp,
     std::size(kEnableExperienceKitMapsWithSrp), nullptr}};

const FeatureEntry::FeatureParam kFollowingFeedSortTypeGroupedByPublisher[] = {
    {kFollowingFeedDefaultSortTypeGroupedByPublisher, "true"},
    {kFollowingFeedDefaultSortTypeSortByLatest, "false"}};
const FeatureEntry::FeatureParam kFollowingFeedSortTypeSortByLatest[] = {
    {kFollowingFeedDefaultSortTypeGroupedByPublisher, "false"},
    {kFollowingFeedDefaultSortTypeSortByLatest, "true"}};

const FeatureEntry::FeatureVariation kFollowingFeedDefaultSortTypeVariations[] =
    {{"Grouped by Publisher", kFollowingFeedSortTypeGroupedByPublisher,
      std::size(kFollowingFeedSortTypeGroupedByPublisher), nullptr},
     {"Sort by Latest", kFollowingFeedSortTypeSortByLatest,
      std::size(kFollowingFeedSortTypeSortByLatest), nullptr}};

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
         syncer::kSyncServiceURL,
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
    {"fullscreen-promos-manager",
     flag_descriptions::kFullscreenPromosManagerName,
     flag_descriptions::kFullscreenPromosManagerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenPromosManager)},
    {"fullscreen-promos-manager-skip-internal-limits",
     flag_descriptions::kFullscreenPromosManagerSkipInternalLimitsName,
     flag_descriptions::kFullscreenPromosManagerSkipInternalLimitsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenPromosManagerSkipInternalLimits)},
    {"fullscreen-viewport-adjustment-experiment",
     flag_descriptions::kFullscreenSmoothScrollingName,
     flag_descriptions::kFullscreenSmoothScrollingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(fullscreen::features::kSmoothScrollingDefault)},
    {"webpage-default-zoom-from-dynamic-type",
     flag_descriptions::kWebPageDefaultZoomFromDynamicTypeName,
     flag_descriptions::kWebPageDefaultZoomFromDynamicTypeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageDefaultZoomFromDynamicType)},
    {"webpage-alternative-text-zoom",
     flag_descriptions::kWebPageAlternativeTextZoomName,
     flag_descriptions::kWebPageAlternativeTextZoomDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(web::kWebPageAlternativeTextZoom)},
    {"webpage-text-zoom-ipad", flag_descriptions::kWebPageTextZoomIPadName,
     flag_descriptions::kWebPageTextZoomIPadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageTextZoomIPad)},
    {"toolbar-container", flag_descriptions::kToolbarContainerName,
     flag_descriptions::kToolbarContainerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(toolbar_container::kToolbarContainerEnabled)},
    {"omnibox-ui-max-autocomplete-matches",
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesName,
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kUIExperimentMaxAutocompleteMatches,
         kOmniboxUIMaxAutocompleteMatchesVariations,
         "OmniboxUIMaxAutocompleteVariations")},
    {"omnibox-local-history-zero-suggest-beyond-ntp",
     flag_descriptions::kOmniboxLocalHistoryZeroSuggestBeyondNTPName,
     flag_descriptions::kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kLocalHistoryZeroSuggestBeyondNTP)},
    {"omnibox-max-zps-matches", flag_descriptions::kOmniboxMaxZPSMatchesName,
     flag_descriptions::kOmniboxMaxZPSMatchesDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kMaxZeroSuggestMatches,
                                    kOmniboxMaxZPSMatchesVariations,
                                    "OmniboxMaxZPSVariations")},
    {"autofill-use-mobile-label-disambiguation",
     flag_descriptions::kAutofillUseMobileLabelDisambiguationName,
     flag_descriptions::kAutofillUseMobileLabelDisambiguationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUseMobileLabelDisambiguation,
         kAutofillUseMobileLabelDisambiguationVariations,
         "AutofillUseMobileLabelDisambiguation")},
    {"ios-breadcrumbs", flag_descriptions::kLogBreadcrumbsName,
     flag_descriptions::kLogBreadcrumbsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(breadcrumbs::kLogBreadcrumbs)},
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kForceStartupSigninPromo)},
    {"identity-status-consistency",
     flag_descriptions::kIdentityStatusConsistencyName,
     flag_descriptions::kIdentityStatusConsistencyDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kIdentityStatusConsistency)},
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
    {"expanded-tab-strip", flag_descriptions::kExpandedTabStripName,
     flag_descriptions::kExpandedTabStripDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kExpandedTabStrip)},
    {"shared-highlighting-ios", flag_descriptions::kSharedHighlightingIOSName,
     flag_descriptions::kSharedHighlightingIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSharedHighlightingIOS)},
    {"new-mobile-identity-consistency-fre",
     flag_descriptions::kNewMobileIdentityConsistencyFREName,
     flag_descriptions::kNewMobileIdentityConsistencyFREDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(signin::kNewMobileIdentityConsistencyFRE,
                                    kNewMobileIdentityConsistencyFREVariations,
                                    kIOSMICeAndDefaultBrowserTrialName)},
#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
    {"screen-time-integration-ios",
     flag_descriptions::kScreenTimeIntegrationName,
     flag_descriptions::kScreenTimeIntegrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kScreenTimeIntegration)},
#endif
    {"modern-tab-strip", flag_descriptions::kModernTabStripName,
     flag_descriptions::kModernTabStripDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kModernTabStrip)},
    {"record-snapshot-size", flag_descriptions::kRecordSnapshotSizeName,
     flag_descriptions::kRecordSnapshotSizeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kRecordSnapshotSize)},
    {"default-browser-fullscreen-promo-experiment",
     flag_descriptions::kDefaultBrowserFullscreenPromoExperimentName,
     flag_descriptions::kDefaultBrowserFullscreenPromoExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kDefaultBrowserFullscreenPromoExperiment,
         kDefaultBrowserFullscreenPromoExperimentVariations,
         "IOSDefaultBrowserFullscreenPromoExperiment")},
    {"ios-shared-highlighting-color-change",
     flag_descriptions::kIOSSharedHighlightingColorChangeName,
     flag_descriptions::kIOSSharedHighlightingColorChangeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIOSSharedHighlightingColorChange)},
    {"ios-shared-highlighting-v2",
     flag_descriptions::kIOSSharedHighlightingV2Name,
     flag_descriptions::kIOSSharedHighlightingV2Description, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(shared_highlighting::kIOSSharedHighlightingV2)},
    {"omnibox-new-textfield-implementation",
     flag_descriptions::kOmniboxNewImplementationName,
     flag_descriptions::kOmniboxNewImplementationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSNewOmniboxImplementation)},
    {"omnibox-on-focus-suggestions-contextual-web",
     flag_descriptions::kOmniboxFocusTriggersContextualWebZeroSuggestName,
     flag_descriptions::
         kOmniboxFocusTriggersContextualWebZeroSuggestDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kFocusTriggersContextualWebZeroSuggest)},
    {"omnibox-on-focus-suggestions-srp",
     flag_descriptions::kOmniboxFocusTriggersSRPZeroSuggestName,
     flag_descriptions::kOmniboxFocusTriggersSRPZeroSuggestDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kFocusTriggersSRPZeroSuggest)},
    {"omnibox-report-assisted-query-stats",
     flag_descriptions::kOmniboxReportAssistedQueryStatsName,
     flag_descriptions::kOmniboxReportAssistedQueryStatsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kReportAssistedQueryStats)},
    {"omnibox-report-searchbox-stats",
     flag_descriptions::kOmniboxReportSearchboxStatsName,
     flag_descriptions::kOmniboxReportSearchboxStatsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kReportSearchboxStats)},
    {"omnibox-fuzzy-url-suggestions",
     flag_descriptions::kOmniboxFuzzyUrlSuggestionsName,
     flag_descriptions::kOmniboxFuzzyUrlSuggestionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxFuzzyUrlSuggestions)},
    {"omnibox-new-popup-ui", flag_descriptions::kIOSOmniboxUpdatedPopupUIName,
     flag_descriptions::kIOSOmniboxUpdatedPopupUIDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSOmniboxUpdatedPopupUI,
                                    kiOSOmniboxUpdatedPopupUIVariations,
                                    "IOSOmniboxUpdatedPopupUI")},
    {"start-surface", flag_descriptions::kStartSurfaceName,
     flag_descriptions::kStartSurfaceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kStartSurface,
                                    kStartSurfaceVariations,
                                    "StartSurface")},
    {"ios-crashpad", flag_descriptions::kCrashpadIOSName,
     flag_descriptions::kCrashpadIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCrashpadIOS)},
    {"autofill-address-verification-in-save-prompt",
     flag_descriptions::kEnableAutofillAddressSavePromptAddressVerificationName,
     flag_descriptions::
         kEnableAutofillAddressSavePromptAddressVerificationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillAddressProfileSavePromptAddressVerificationSupport)},
    {"filling-across-affiliated-websites",
     flag_descriptions::kFillingAcrossAffiliatedWebsitesName,
     flag_descriptions::kFillingAcrossAffiliatedWebsitesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kFillingAcrossAffiliatedWebsites)},
    {"interest-feed-v2-clicks-and-views-cond-upload",
     flag_descriptions::kInterestFeedV2ClickAndViewActionsConditionalUploadName,
     flag_descriptions::
         kInterestFeedV2ClickAndViewActionsConditionalUploadDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2ClicksAndViewsConditionalUpload)},
    {"incognito-ntp-revamp", flag_descriptions::kIncognitoNtpRevampName,
     flag_descriptions::kIncognitoNtpRevampDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIncognitoNtpRevamp)},
    {"update-history-entry-points-in-incognito",
     flag_descriptions::kUpdateHistoryEntryPointsInIncognitoName,
     flag_descriptions::kUpdateHistoryEntryPointsInIncognitoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUpdateHistoryEntryPointsInIncognito)},
    {"sync-trusted-vault-passphrase-promo",
     flag_descriptions::kSyncTrustedVaultPassphrasePromoName,
     flag_descriptions::kSyncTrustedVaultPassphrasePromoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(::syncer::kSyncTrustedVaultPassphrasePromo)},
    {"sync-trusted-vault-passphrase-recovery",
     flag_descriptions::kSyncTrustedVaultPassphraseRecoveryName,
     flag_descriptions::kSyncTrustedVaultPassphraseRecoveryDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(::syncer::kSyncTrustedVaultPassphraseRecovery)},
    {"wait-threshold-seconds-for-capabilities-api",
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiName,
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kWaitThresholdMillisecondsForCapabilitiesApiChoices)},
    {"autofill-fill-merchant-promo-code-fields",
     flag_descriptions::kAutofillFillMerchantPromoCodeFieldsName,
     flag_descriptions::kAutofillFillMerchantPromoCodeFieldsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillFillMerchantPromoCodeFields)},
    {"new-overflow-menu", flag_descriptions::kNewOverflowMenuName,
     flag_descriptions::kNewOverflowMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewOverflowMenu)},
    {"new-overflow-menu-alternate-iph",
     flag_descriptions::kNewOverflowMenuAlternateIPHName,
     flag_descriptions::kNewOverflowMenuAlternateIPHDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNewOverflowMenuAlternateIPH)},
    {"use-lens-to-search-for-image",
     flag_descriptions::kUseLensToSearchForImageName,
     flag_descriptions::kUseLensToSearchForImageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseLensToSearchForImage)},
    {"enable-lens-in-home-screen-widget",
     flag_descriptions::kEnableLensInHomeScreenWidgetName,
     flag_descriptions::kEnableLensInHomeScreenWidgetDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableLensInHomeScreenWidget)},
    {"enable-lens-in-keyboard", flag_descriptions::kEnableLensInKeyboardName,
     flag_descriptions::kEnableLensInKeyboardDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLensInKeyboard)},
    {"enable-lens-in-ntp", flag_descriptions::kEnableLensInNTPName,
     flag_descriptions::kEnableLensInNTPDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLensInNTP)},
    {"enable-lens-in-omnibox-copied-image",
     flag_descriptions::kEnableLensInOmniboxCopiedImageName,
     flag_descriptions::kEnableLensInOmniboxCopiedImageDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableLensInOmniboxCopiedImage)},
    {"use-load-simulated-request-for-error-page-navigation",
     flag_descriptions::kUseLoadSimulatedRequestForOfflinePageName,
     flag_descriptions::kUseLoadSimulatedRequestForOfflinePageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kUseLoadSimulatedRequestForOfflinePage)},
    {"enable-disco-feed-endpoint",
     flag_descriptions::kEnableDiscoverFeedDiscoFeedEndpointName,
     flag_descriptions::kEnableDiscoverFeedDiscoFeedEndpointDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableDiscoverFeedDiscoFeedEndpoint)},
    {"enable-discover-feed-top-sync-promo",
     flag_descriptions::kEnableDiscoverFeedTopSyncPromoName,
     flag_descriptions::kEnableDiscoverFeedTopSyncPromoDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableDiscoverFeedTopSyncPromo,
                                    kDiscoverFeedTopSyncPromoVariations,
                                    "EnableDiscoverFeedTopSyncPromo")},
    {"enable-fre-default-browser-screen-testing",
     flag_descriptions::kEnableFREDefaultBrowserPromoScreenName,
     flag_descriptions::kEnableFREDefaultBrowserPromoScreenDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFREDefaultBrowserPromoScreen,
                                    kFREDefaultBrowserPromoVariations,
                                    kIOSMICeAndDefaultBrowserTrialName)},
    {"shared-highlighting-amp",
     flag_descriptions::kIOSSharedHighlightingAmpName,
     flag_descriptions::kIOSSharedHighlightingAmpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingAmp)},
    {"enable-commerce-price-tracking",
     commerce::flag_descriptions::kCommercePriceTrackingName,
     commerce::flag_descriptions::kCommercePriceTrackingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kCommercePriceTracking,
                                    commerce::kCommercePriceTrackingVariations,
                                    "CommercePriceTracking")},
    {"web-feed-ios", flag_descriptions::kEnableWebChannelsName,
     flag_descriptions::kEnableWebChannelsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableWebChannels)},
    {"ntp-view-hierarchy-repair",
     flag_descriptions::kNTPViewHierarchyRepairName,
     flag_descriptions::kNTPViewHierarchyRepairDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableNTPViewHierarchyRepair)},
    {"synthesized-restore-session",
     flag_descriptions::kSynthesizedRestoreSessionName,
     flag_descriptions::kSynthesizedRestoreSessionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kSynthesizedRestoreSession)},
    {"enable-password-manager-branding-update",
     flag_descriptions::kIOSEnablePasswordManagerBrandingUpdateName,
     flag_descriptions::kIOSEnablePasswordManagerBrandingUpdateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kIOSEnablePasswordManagerBrandingUpdate)},
    {"ios-media-permissions-control",
     flag_descriptions::kMediaPermissionsControlName,
     flag_descriptions::kMediaPermissionsControlDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kMediaPermissionsControl)},
    {"enable-save-session-tabs-in-separate-files",
     flag_descriptions::kSaveSessionTabsToSeparateFilesName,
     flag_descriptions::kSaveSessionTabsToSeparateFilesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(sessions::kSaveSessionTabsToSeparateFiles)},
    {"use-sf-symbols", flag_descriptions::kUseSFSymbolsName,
     flag_descriptions::kUseSFSymbolsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseSFSymbols)},
    {"enable-unicorn-account-support",
     flag_descriptions::kEnableUnicornAccountSupportName,
     flag_descriptions::kEnableUnicornAccountSupportDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(signin::kEnableUnicornAccountSupport)},
    {"ios-webpage-intent-annotations",
     flag_descriptions::kEnableWebPageAnnotationsName,
     flag_descriptions::kEnableWebPageAnnotationsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableWebPageAnnotations)},
    {"enable-password-grouping", flag_descriptions::kPasswordsGroupingName,
     flag_descriptions::kPasswordsGroupingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kPasswordsGrouping)},
    {"enable-fullscreen-api", flag_descriptions::kEnableFullscreenAPIName,
     flag_descriptions::kEnableFullscreenAPIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableFullscreenAPI)},
    {"enable-tailored-security-integration",
     flag_descriptions::kTailoredSecurityIntegrationName,
     flag_descriptions::kTailoredSecurityIntegrationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(safe_browsing::kTailoredSecurityIntegration)},
    {"autofill-enable-card-product-name",
     flag_descriptions::kAutofillEnableCardProductNameName,
     flag_descriptions::kAutofillEnableCardProductNameDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardProductName)},
    {"send-tab-to-self-signin-promo",
     flag_descriptions::kSendTabToSelfSigninPromoName,
     flag_descriptions::kSendTabToSelfSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfSigninPromo)},
    {"bubble-rich-iph", flag_descriptions::kBubbleRichIPHName,
     flag_descriptions::kBubbleRichIPHDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBubbleRichIPH,
                                    kBubbleRichIPHVariations,
                                    "BubbleRichIPH")},
    {"autofill-enforce-delays-in-strike-database",
     flag_descriptions::kAutofillEnforceDelaysInStrikeDatabaseName,
     flag_descriptions::kAutofillEnforceDelaysInStrikeDatabaseDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceDelaysInStrikeDatabase)},
    {"autofill-upstream-allow-additional-email-domains",
     flag_descriptions::kAutofillUpstreamAllowAdditionalEmailDomainsName,
     flag_descriptions::kAutofillUpstreamAllowAdditionalEmailDomainsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamAllowAdditionalEmailDomains)},
    {"autofill-upstream-allow-all-email-domains",
     flag_descriptions::kAutofillUpstreamAllowAllEmailDomainsName,
     flag_descriptions::kAutofillUpstreamAllowAllEmailDomainsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamAllowAllEmailDomains)},
    {"enable-download-service-foreground-session",
     flag_descriptions::kDownloadServiceForegroundSessionName,
     flag_descriptions::kDownloadServiceForegroundSessionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(download::kDownloadServiceForegroundSessionIOSFeature)},
    {"enable-tflite-language-detection",
     flag_descriptions::kTFLiteLanguageDetectionName,
     flag_descriptions::kTFLiteLanguageDetectionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(translate::kTFLiteLanguageDetectionEnabled)},
    {"optimization-guide-debug-logs",
     flag_descriptions::kOptimizationGuideDebugLogsName,
     flag_descriptions::kOptimizationGuideDebugLogsDescription,
     flags_ui::kOsIos,
     SINGLE_VALUE_TYPE(optimization_guide::switches::kDebugLoggingEnabled)},
    {"optimization-guide-model-downloading",
     flag_descriptions::kOptimizationGuideModelDownloadingName,
     flag_descriptions::kOptimizationGuideModelDownloadingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         optimization_guide::features::kOptimizationGuideModelDownloading)},
    {"optimization-target-prediction",
     flag_descriptions::kOptimizationTargetPredictionName,
     flag_descriptions::kOptimizationTargetPredictionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         optimization_guide::features::kOptimizationTargetPrediction)},
    {"sync-standalone-invalidations", flag_descriptions::kSyncInvalidationsName,
     flag_descriptions::kSyncInvalidationsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(::syncer::kUseSyncInvalidations)},
    {"sync-standalone-invalidations-wallet-and-offer",
     flag_descriptions::kSyncInvalidationsWalletAndOfferName,
     flag_descriptions::kSyncInvalidationsWalletAndOfferDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(::syncer::kUseSyncInvalidationsForWalletAndOffer)},
    {"suggestions-scrolling-ipad",
     flag_descriptions::kEnableSuggestionsScrollingOnIPadName,
     flag_descriptions::kEnableSuggestionsScrollingOnIPadDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableSuggestionsScrollingOnIPad)},
    {"popout-omnibox-ipad", flag_descriptions::kEnablePopoutOmniboxIpadName,
     flag_descriptions::kEnablePopoutOmniboxIpadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnablePopoutOmniboxIpad)},
    {"experience-kit-calendar", flag_descriptions::kCalendarExperienceKitName,
     flag_descriptions::kCalendarExperienceKitDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCalendarExperienceKit)},
    {"intents-on-phone-number", flag_descriptions::kPhoneNumberName,
     flag_descriptions::kPhoneNumberDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnablePhoneNumbers)},
    {"experience-kit-apple-calendar",
     flag_descriptions::kAppleCalendarExperienceKitName,
     flag_descriptions::kAppleCalendarExperienceKitDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableExpKitAppleCalendar)},
    {"enable-expkit-calendar-text-classifier",
     flag_descriptions::kEnableExpKitCalendarTextClassifierName,
     flag_descriptions::kEnableExpKitCalendarTextClassifierDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableExpKitCalendarTextClassifier)},
    {"enable-expkit-text-classifier",
     flag_descriptions::kEnableExpKitTextClassifierName,
     flag_descriptions::kEnableExpKitTextClassifierDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableExpKitTextClassifier)},
    {"experience-kit-maps", flag_descriptions::kMapsExperienceKitName,
     flag_descriptions::kMapsExperienceKitDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kMapsExperienceKit,
                                    kEnableExperienceKitMapsVariations,
                                    "IOSExperienceKitMaps")},
    {"enable-long-press-surrounding-text",
     flag_descriptions::kLongPressSurroundingTextName,
     flag_descriptions::kLongPressSurroundingTextDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kLongPressSurroundingText)},
    {"https-only-mode", flag_descriptions::kHttpsOnlyModeName,
     flag_descriptions::kHttpsOnlyModeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(security_interstitials::features::kHttpsOnlyMode)},
    {"omnibox-https-upgrades", flag_descriptions::kOmniboxHttpsUpgradesName,
     flag_descriptions::kOmniboxHttpsUpgradesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kDefaultTypedNavigationsToHttps)},
    {"smart-sorting-new-overflow-menu",
     flag_descriptions::kSmartSortingNewOverflowMenuName,
     flag_descriptions::kSmartSortingNewOverflowMenuDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kSmartSortingNewOverflowMenu)},
    {"new-overflow-menu-share-chrome-action",
     flag_descriptions::kNewOverflowMenuShareChromeActionName,
     flag_descriptions::kNewOverflowMenuShareChromeActionDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNewOverflowMenuShareChromeAction)},
    {"autofill-enable-ranking-formula",
     flag_descriptions::kAutofillEnableRankingFormulaName,
     flag_descriptions::kAutofillEnableRankingFormulaDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableRankingFormula)},
    {"enable-feed-ablation", flag_descriptions::kEnableFeedAblationName,
     flag_descriptions::kEnableFeedAblationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFeedAblation)},
    {"enable-feed-bottom-sign-in-promo",
     flag_descriptions::kEnableFeedBottomSignInPromoName,
     flag_descriptions::kEnableFeedBottomSignInPromoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableFeedBottomSignInPromo)},
    {"enable-feed-card-menu-sign-in-promo",
     flag_descriptions::kEnableFeedCardMenuSignInPromoName,
     flag_descriptions::kEnableFeedCardMenuSignInPromoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableFeedCardMenuSignInPromo)},
    {"content-suggestions-ui-module-refresh",
     flag_descriptions::kContentSuggestionsUIModuleRefreshName,
     flag_descriptions::kContentSuggestionsUIModuleRefreshDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kContentSuggestionsUIModuleRefresh,
         kModuleRefreshVariations,
         kContentSuggestionsUIModuleRefreshFlagOverrideFieldTrialName)},
    {"3p-intents-in-incognito", flag_descriptions::kIOS3PIntentsInIncognitoName,
     flag_descriptions::kIOS3PIntentsInIncognitoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOS3PIntentsInIncognito)},
    {"default-browser-intents-show-settings",
     flag_descriptions::kDefaultBrowserIntentsShowSettingsName,
     flag_descriptions::kDefaultBrowserIntentsShowSettingsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDefaultBrowserIntentsShowSettings)},
    {"enable-discover-feed-ghost-cards",
     flag_descriptions::kEnableDiscoverFeedGhostCardsName,
     flag_descriptions::kEnableDiscoverFeedGhostCardsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableDiscoverFeedGhostCards)},
    {"dm-token-deletion", flag_descriptions::kDmTokenDeletionName,
     flag_descriptions::kDmTokenDeletionDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(policy::features::kDmTokenDeletion,
                                    kDmTokenDeletionVariation,
                                    "DmTokenDeletion")},
    {"ios-password-ui-split", flag_descriptions::kIOSPasswordUISplitName,
     flag_descriptions::kIOSPasswordUISplitDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSPasswordUISplit)},
    {"ios-password-manager-cross-origin-iframe-support",
     flag_descriptions::kIOSPasswordManagerCrossOriginIframeSupportName,
     flag_descriptions::kIOSPasswordManagerCrossOriginIframeSupportDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kIOSPasswordManagerCrossOriginIframeSupport)},
    {"ios-popular-sites-improved-suggestions",
     flag_descriptions::kIOSPopularSitesImprovedSuggestionsName,
     flag_descriptions::kIOSPopularSitesImprovedSuggestionsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         ntp_tiles::kIOSPopularSitesImprovedSuggestions,
         kIOSPopularSitesImprovedSuggestionsVariations,
         field_trial_constants::
             kIOSPopularSitesImprovedSuggestionsFieldTrialName)},
    {"omnibox-adaptive-suggestions-count",
     flag_descriptions::kAdaptiveSuggestionsCountName,
     flag_descriptions::kAdaptiveSuggestionsCountDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kAdaptiveSuggestionsCount)},
    {"trending-queries-module", flag_descriptions::kTrendingQueriesModuleName,
     flag_descriptions::kTrendingQueriesModuleDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kTrendingQueriesModule,
         kTrendingQueriesModuleVariations,
         kTrendingQueriesFlagOverrideFieldTrialName)},
    {"autofill-parse-iban-fields",
     flag_descriptions::kAutofillParseIBANFieldsName,
     flag_descriptions::kAutofillParseIBANFieldsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillParseIBANFields)},
    {"autofill-enable-new-card-unmask-prompt-view",
     flag_descriptions::kAutofillEnableNewCardUnmaskPromptViewName,
     flag_descriptions::kAutofillEnableNewCardUnmaskPromptViewDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableNewCardUnmaskPromptView)},
    {"omnibox-paste-button", flag_descriptions::kOmniboxPasteButtonName,
     flag_descriptions::kOmniboxPasteButtonDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kOmniboxPasteButton,
                                    kOmniboxPasteButtonVariations,
                                    "OmniboxPasteButton")},
    {"omnibox-zero-suggest-prefetching",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kZeroSuggestPrefetching)},
    {"omnibox-zero-suggest-prefetching-on-srp",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnSRPName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnSRPDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestPrefetchingOnSRP)},
    {"omnibox-zero-suggest-prefetching-on-web",
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnWebName,
     flag_descriptions::kOmniboxZeroSuggestPrefetchingOnWebDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestPrefetchingOnWeb)},
    {"omnibox-zero-suggest-in-memory-caching",
     flag_descriptions::kOmniboxZeroSuggestInMemoryCachingName,
     flag_descriptions::kOmniboxZeroSuggestInMemoryCachingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestInMemoryCaching)},
    {"omnibox-on-device-tail-suggestions",
     flag_descriptions::kOmniboxOnDeviceTailSuggestionsName,
     flag_descriptions::kOmniboxOnDeviceTailSuggestionsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kOnDeviceTailModel)},
    {"enable-user-policy", flag_descriptions::kEnableUserPolicyName,
     flag_descriptions::kEnableUserPolicyDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(policy::kUserPolicy)},
    {"enable-sync-history-datatype",
     flag_descriptions::kSyncEnableHistoryDataTypeName,
     flag_descriptions::kSyncEnableHistoryDataTypeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncEnableHistoryDataType)},
    {"omnibox-max-url-matches", flag_descriptions::kOmniboxMaxURLMatchesName,
     flag_descriptions::kOmniboxMaxURLMatchesDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxMaxURLMatches,
                                    kOmniboxMaxURLMatchesVariations,
                                    "OmniboxMaxURLMatches")},
    {"metrickit-non-crash-reports",
     flag_descriptions::kMetrickitNonCrashReportName,
     flag_descriptions::kMetrickitNonCrashReportDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMetrickitNonCrashReport)},
    {"autofill-enable-remade-downstream-metrics",
     flag_descriptions::kAutofillEnableRemadeDownstreamMetricsName,
     flag_descriptions::kAutofillEnableRemadeDownstreamMetricsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableRemadeDownstreamMetrics)},
    {"autofill-parse-vcn-card-on-file-standalone-cvc-fields",
     flag_descriptions::kAutofillParseVcnCardOnFileStandaloneCvcFieldsName,
     flag_descriptions::
         kAutofillParseVcnCardOnFileStandaloneCvcFieldsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillParseVcnCardOnFileStandaloneCvcFields)},
#if BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"feed-background-refresh-ios",
     flag_descriptions::kFeedBackgroundRefreshName,
     flag_descriptions::kFeedBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFeedBackgroundRefresh,
                                    kFeedBackgroundRefreshVariations,
                                    "FeedBackgroundRefresh")},
    {"omnibox-keyboard-paste-button",
     flag_descriptions::kOmniboxKeyboardPasteButtonName,
     flag_descriptions::kOmniboxKeyboardPasteButtonDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kOmniboxKeyboardPasteButton)},
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"enable-cbd-sign-out", flag_descriptions::kEnableCBDSignOutName,
     flag_descriptions::kEnableCBDSignOutDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kEnableCbdSignOut)},
    {"enable-open-in-download", flag_descriptions::kEnableOpenInDownloadName,
     flag_descriptions::kEnableOpenInDownloadDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableOpenInDownload,
                                    kOpenInDownloadVariations,
                                    "EnableOpenInDownload")},
    {"whats-new-ios", flag_descriptions::kWhatsNewIOSName,
     flag_descriptions::kWhatsNewIOSDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kWhatsNewIOS,
                                    kWhatsNewLayoutVariations,
                                    "WhatsNewLayoutVariations")},
    {"ios-autofill-branding", flag_descriptions::kAutofillBrandingIOSName,
     flag_descriptions::kAutofillBrandingIOSDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(autofill::features::kAutofillBrandingIOS,
                                    kAutofillBrandingIOSVariations,
                                    "AutofillBrandingIOS")},
    {"app-store-rating", flag_descriptions::kAppStoreRatingName,
     flag_descriptions::kAppStoreRatingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAppStoreRating)},
    {"ios-new-post-restore-experience",
     flag_descriptions::kIOSNewPostRestoreExperienceName,
     flag_descriptions::kIOSNewPostRestoreExperienceDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         post_restore_signin::features::kIOSNewPostRestoreExperience,
         kIOSNewPostRestoreExperienceVariations,
         "IOSNewPostRestoreExperience")},
    {"most-visited-tiles", flag_descriptions::kMostVisitedTilesName,
     flag_descriptions::kMostVisitedTilesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kMostVisitedTiles)},
    {"enable-tflite-language-detection-ignore",
     flag_descriptions::kTFLiteLanguageDetectionIgnoreName,
     flag_descriptions::kTFLiteLanguageDetectionIgnoreDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(translate::kTFLiteLanguageDetectionIgnoreEnabled)},
    {"keyboard-shortcuts-menu", flag_descriptions::kKeyboardShortcutsMenuName,
     flag_descriptions::kKeyboardShortcutsMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kKeyboardShortcutsMenu)},
    {"enable-check-visibility-on-attention-log-start",
     flag_descriptions::kEnableCheckVisibilityOnAttentionLogStartName,
     flag_descriptions::kEnableCheckVisibilityOnAttentionLogStartDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableCheckVisibilityOnAttentionLogStart)},
    {"enable-refine-data-source-reload-reporting",
     flag_descriptions::kEnableRefineDataSourceReloadReportingName,
     flag_descriptions::kEnableRefineDataSourceReloadReportingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableRefineDataSourceReloadReporting)},
    {"enable-compromised-passwords-muting",
     flag_descriptions::kEnableCompromisedPasswordsMutingName,
     flag_descriptions::kEnableCompromisedPasswordsMutingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kMuteCompromisedPasswords)},
    {"enable-passwords-account-storage",
     flag_descriptions::kEnablePasswordsAccountStorageName,
     flag_descriptions::kEnablePasswordsAccountStorageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnablePasswordsAccountStorage)},
    {"enable-default-following-feed-sort-type",
     flag_descriptions::kFollowingFeedDefaultSortTypeName,
     flag_descriptions::kFollowingFeedDefaultSortTypeDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFollowingFeedDefaultSortType,
                                    kFollowingFeedDefaultSortTypeVariations,
                                    "EnableFollowingFeedDefaultSortType")},
    {"omnibox-carousel-dynamic-spacing",
     flag_descriptions::kOmniboxCarouselDynamicSpacingName,
     flag_descriptions::kOmniboxCarouselDynamicSpacingDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kOmniboxCarouselDynamicSpacing)},
    {"use-sf-symbols-omnibox", flag_descriptions::kUseSFSymbolsInOmniboxName,
     flag_descriptions::kUseSFSymbolsInOmniboxDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseSFSymbolsInOmnibox)},
    {"tab-grid-sort", flag_descriptions::kTabGridRecencySortName,
     flag_descriptions::kTabGridRecencySortDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridRecencySort)},
    {"enable-pinned-tabs", flag_descriptions::kEnablePinnedTabsName,
     flag_descriptions::kEnablePinnedTabsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnablePinnedTabs,
                                    kEnablePinnedTabsVariations,
                                    "EnablePinnedTabs")},
    {"remove-crash-infobar", flag_descriptions::kRemoveCrashInfobarName,
     flag_descriptions::kRemoveCrashInfobarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kRemoveCrashInfobar)},
    {"credential-provider-extension-promo",
     flag_descriptions::kCredentialProviderExtensionPromoName,
     flag_descriptions::kCredentialProviderExtensionPromoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCredentialProviderExtensionPromo)},
    {"default-browser-blue-dot-promo",
     flag_descriptions::kDefaultBrowserBlueDotPromoName,
     flag_descriptions::kDefaultBrowserBlueDotPromoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDefaultBrowserBlueDotPromo)},
    {"ios-force-translate-enabled",
     flag_descriptions::kIOSForceTranslateEnabledName,
     flag_descriptions::kIOSForceTranslateEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(translate::kIOSForceTranslateEnabled)},
    {"iph-price-notifications-while-browsing",
     flag_descriptions::kIPHPriceNotificationsWhileBrowsingName,
     flag_descriptions::kIPHPriceNotificationsWhileBrowsingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         feature_engagement::kIPHPriceNotificationsWhileBrowsingFeature)},
};

bool SkipConditionalFeatureEntry(const flags_ui::FeatureEntry& entry) {
  return false;
}

flags_ui::FlagsState& GetGlobalFlagsState() {
  static base::NoDestructor<flags_ui::FlagsState> flags_state(kFeatureEntries,
                                                              nullptr);
  return *flags_state;
}
// Creates the experimental test policies map, used by AsyncPolicyLoader and
// PolicyLoaderIOS to locally enable policies.
NSMutableDictionary* CreateExperimentalTestingPolicies() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Shared variables for all enterprise experimental flags.
  NSMutableDictionary* testing_policies = [[NSMutableDictionary alloc] init];
  NSMutableArray* allowed_experimental_policies = [[NSMutableArray alloc] init];

  // Set some sample policy values for testing if EnableSamplePolicies is set to
  // true.
  if ([defaults boolForKey:@"EnableSamplePolicies"]) {
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

      base::SysUTF8ToNSString(policy::key::kAppStoreRatingEnabled) : @NO,
    }];
  }

  if ([defaults boolForKey:@"EnableSyncDisabledPolicy"]) {
    NSString* sync_policy_key =
        base::SysUTF8ToNSString(policy::key::kSyncDisabled);
    [testing_policies addEntriesFromDictionary:@{sync_policy_key : @YES}];
  }

  // SyncTypesListDisabled policy.
  NSString* Sync_types_list_disabled_key =
      base::SysUTF8ToNSString(policy::key::kSyncTypesListDisabled);
  NSMutableArray* Sync_types_list_disabled_values =
      [[NSMutableArray alloc] init];
  if ([defaults boolForKey:@"SyncTypesListBookmarks"]) {
    [Sync_types_list_disabled_values addObject:@"bookmarks"];
  }
  if ([defaults boolForKey:@"SyncTypesListReadingList"]) {
    [Sync_types_list_disabled_values addObject:@"readingList"];
  }
  if ([defaults boolForKey:@"SyncTypesListPreferences"]) {
    [Sync_types_list_disabled_values addObject:@"preferences"];
  }
  if ([defaults boolForKey:@"SyncTypesListPasswords"]) {
    [Sync_types_list_disabled_values addObject:@"passwords"];
  }
  if ([defaults boolForKey:@"SyncTypesListAutofill"]) {
    [Sync_types_list_disabled_values addObject:@"autofill"];
  }
  if ([defaults boolForKey:@"SyncTypesListTypedUrls"]) {
    [Sync_types_list_disabled_values addObject:@"typedUrls"];
  }
  if ([defaults boolForKey:@"SyncTypesListTabs"]) {
    [Sync_types_list_disabled_values addObject:@"tabs"];
  }
  if ([Sync_types_list_disabled_values count]) {
    [testing_policies addEntriesFromDictionary:@{
      Sync_types_list_disabled_key : Sync_types_list_disabled_values
    }];
  }

  // If an incognito mode availability is set, set the value.
  NSString* incognito_policy_key =
      base::SysUTF8ToNSString(policy::key::kIncognitoModeAvailability);
  NSInteger incognito_mode_availability =
      [defaults integerForKey:incognito_policy_key];
  if (incognito_mode_availability) {
    [testing_policies addEntriesFromDictionary:@{
      incognito_policy_key : @(incognito_mode_availability),
    }];
  }

  NSString* restriction_pattern =
      [defaults stringForKey:@"RestrictAccountsToPatterns"];
  if ([restriction_pattern length] > 0) {
    NSString* restrict_key =
        base::SysUTF8ToNSString(policy::key::kRestrictAccountsToPatterns);
    [testing_policies addEntriesFromDictionary:@{
      restrict_key : @[ restriction_pattern ]
    }];
  }

  // If the sign-in policy is set (not "None"), add the policy key to the list
  // of enabled experimental policies, and set the value.
  NSString* const kSigninPolicyKey = @"BrowserSignin";
  NSInteger signin_policy_mode = [defaults integerForKey:kSigninPolicyKey];
  if (signin_policy_mode) {
    // Remove the mode offset that was used to represent the unset policy.
    --signin_policy_mode;
    DCHECK(signin_policy_mode >= 0);

    [testing_policies addEntriesFromDictionary:@{
      kSigninPolicyKey : @(signin_policy_mode),
    }];
  }

  // If the New Tab Page URL is set (not empty) add the value to the list of
  // test policies.
  NSString* ntp_location = [defaults stringForKey:@"NTPLocation"];
  if ([ntp_location length] > 0) {
    NSString* ntp_location_key =
        base::SysUTF8ToNSString(policy::key::kNewTabPageLocation);
    [testing_policies
        addEntriesFromDictionary:@{ntp_location_key : ntp_location}];
    [allowed_experimental_policies addObject:ntp_location_key];
  }

  if ([defaults boolForKey:@"DisallowChromeDataInBackups"]) {
    NSString* allow_backups_key =
        base::SysUTF8ToNSString(policy::key::kAllowChromeDataInBackups);
    [testing_policies addEntriesFromDictionary:@{allow_backups_key : @NO}];
    [allowed_experimental_policies addObject:allow_backups_key];
  }

  // If any experimental policy was allowed, set the EnableExperimentalPolicies
  // policy.
  if ([allowed_experimental_policies count] > 0) {
    [testing_policies setValue:allowed_experimental_policies
                        forKey:base::SysUTF8ToNSString(
                                   policy::key::kEnableExperimentalPolicies)];
  }

  NSString* metrics_reporting_key = @"MetricsReportingEnabled";
  switch ([defaults integerForKey:metrics_reporting_key]) {
    case 1:
      // Metrics reporting forced.
      [testing_policies setValue:@(YES) forKey:metrics_reporting_key];
      break;
    case 2:
      // Metrics reporting disabled.
      [testing_policies setValue:@(NO) forKey:metrics_reporting_key];
      break;
    default:
      // Metrics reporting not managed.
      break;
  }

  // Warning: Add new flags to TestingPoliciesHash() below.

  return testing_policies;
}

}  // namespace

// Add all switches from experimental flags to `command_line`.
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
  NSMutableDictionary* testing_policies = CreateExperimentalTestingPolicies();

  // If a CBCM enrollment token is provided, force Chrome Browser Cloud
  // Management to enabled and add the token to the list of policies.
  NSString* token_key =
      base::SysUTF8ToNSString(policy::key::kCloudManagementEnrollmentToken);
  NSString* token = [defaults stringForKey:token_key];

  if ([token length] > 0) {
    command_line->AppendSwitch(switches::kEnableChromeBrowserCloudManagement);
    [testing_policies setValue:token forKey:token_key];
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

  ios::provider::AppendSwitchesFromExperimentalSettings(defaults, command_line);
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
  return GetGlobalFlagsState().RegisterAllFeatureVariationParameters(
      flags_storage, feature_list);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::Value::List& supported_entries,
                           base::Value::List& unsupported_entries) {
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

base::span<const flags_ui::FeatureEntry> GetFeatureEntries() {
  return base::span<const flags_ui::FeatureEntry>(kFeatureEntries,
                                                  std::size(kFeatureEntries));
}

}  // namespace testing
