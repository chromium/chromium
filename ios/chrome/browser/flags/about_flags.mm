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
#import "base/check_op.h"
#import "base/debug/debugging_buildflags.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/mac/foundation_util.h"
#import "base/no_destructor.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/core/common/autofill_switches.h"
#import "components/autofill/ios/browser/autofill_switches.h"
#import "components/bookmarks/common/bookmark_features.h"
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
#import "components/history/core/browser/features.h"
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
#import "components/reading_list/features/reading_list_switches.h"
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
#import "components/variations/service/google_groups_updater_service.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/bring_android_tabs/features.h"
#import "ios/chrome/browser/browsing_data/browsing_data_features.h"
#import "ios/chrome/browser/crash_report/features.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/find_in_page/features.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/follow/follow_features.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/features.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/policy/cloud/user_policy_constants.h"
#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/screen_time/screen_time_buildflags.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/text_selection/text_selection_util.h"
#import "ios/chrome/browser/ui/app_store_rating/features.h"
#import "ios/chrome/browser/ui/autofill/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/default_promo/post_restore/features.h"
#import "ios/chrome/browser/ui/download/features.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_field_trial_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
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

// Uses int values from DefaultPromoType enum.
const FeatureEntry::Choice kDefaultBrowserPromoForceShowPromoChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"Show generic promo", "default-browser-promo-force-show-promo", "0"},
    {"Show tailored stay safe promo", "default-browser-promo-force-show-promo",
     "1"},
    {"Show tailored made for ios promo",
     "default-browser-promo-force-show-promo", "2"},
    {"Show tailored all tabs promo", "default-browser-promo-force-show-promo",
     "3"},
    {"Show video promo", "default-browser-promo-force-show-promo", "4"},
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

const FeatureEntry::FeatureParam kDefaultBrowserVideoPromoHalfscreen[] = {
    {"default_browser_video_promo_halfscreen", "true"}};
const FeatureEntry::FeatureParam kDefaultBrowserVideoPromoFullscreen[] = {
    {"default_browser_video_promo_halfscreen", "false"}};
const FeatureEntry::FeatureVariation kDefaultBrowserVideoPromoVariations[] = {
    {"Show half screen ui", kDefaultBrowserVideoPromoHalfscreen,
     std::size(kDefaultBrowserVideoPromoHalfscreen), nullptr},
    {"Show full screen ui", kDefaultBrowserVideoPromoFullscreen,
     std::size(kDefaultBrowserVideoPromoFullscreen), nullptr},
};

const FeatureEntry::FeatureParam
    kDefaultBrowserTriggerCriteriaExperimentParamOptions[] = {
        {kDefaultBrowserTriggerOnOmniboxCopyPaste, "true"}};
const FeatureEntry::FeatureVariation
    kDefaultBrowserTriggerCriteriaExperimentParams[] = {
        {"Trigger on omnibox copy-paste",
         kDefaultBrowserTriggerCriteriaExperimentParamOptions,
         std::size(kDefaultBrowserTriggerCriteriaExperimentParamOptions),
         nullptr},
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

// Uses int values from SigninPromoViewStyle enum.
const FeatureEntry::FeatureParam kDiscoverFeedTopSyncPromoStandard[] = {
    {kDiscoverFeedTopSyncPromoStyle, "0"}};
const FeatureEntry::FeatureParam kDiscoverFeedTopSyncPromoCompactTitled[] = {
    {kDiscoverFeedTopSyncPromoStyle, "1"}};
const FeatureEntry::FeatureParam kDiscoverFeedTopSyncPromoCompactHorizontal[] =
    {{kDiscoverFeedTopSyncPromoStyle, "2"}};
const FeatureEntry::FeatureParam kDiscoverFeedTopSyncPromoCompactVertical[] = {
    {kDiscoverFeedTopSyncPromoStyle, "3"}};

const FeatureEntry::FeatureVariation kDiscoverFeedTopSyncPromoVariations[] = {
    {"Standard", kDiscoverFeedTopSyncPromoStandard,
     std::size(kDiscoverFeedTopSyncPromoStandard), nullptr},
    {"Compact Titled (Unpersonalized)", kDiscoverFeedTopSyncPromoCompactTitled,
     std::size(kDiscoverFeedTopSyncPromoCompactTitled), nullptr},
    {"Compact Horizontal", kDiscoverFeedTopSyncPromoCompactHorizontal,
     std::size(kDiscoverFeedTopSyncPromoCompactHorizontal), nullptr},
    {"Compact Vertical", kDiscoverFeedTopSyncPromoCompactVertical,
     std::size(kDiscoverFeedTopSyncPromoCompactVertical), nullptr}};

const FeatureEntry::FeatureParam kFeedHeaderSettingDisabledStickyHeader[] = {
    {kDisableStickyHeaderForFollowingFeed, "true"}};
const FeatureEntry::FeatureParam kFeedHeaderSettingReducedHeight[] = {
    {kOverrideFeedHeaderHeight, "30"}};
const FeatureEntry::FeatureParam kFeedHeaderSettingAllImprovements[] = {
    {kDisableStickyHeaderForFollowingFeed, "true"},
    {kOverrideFeedHeaderHeight, "30"}};

const FeatureEntry::FeatureVariation kFeedHeaderSettingsVariations[] = {
    {"Disable sticky header", kFeedHeaderSettingDisabledStickyHeader,
     std::size(kFeedHeaderSettingDisabledStickyHeader), nullptr},
    {"Reduced header height", kFeedHeaderSettingReducedHeight,
     std::size(kFeedHeaderSettingReducedHeight), nullptr},
    {"All improvements", kFeedHeaderSettingAllImprovements,
     std::size(kFeedHeaderSettingAllImprovements), nullptr}};

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

const FeatureEntry::FeatureParam kMagicStackMostVisitedModule[] = {
    {kMagicStackMostVisitedModuleParam, "true"},
    {kReducedSpaceParam, "-80"}};
const FeatureEntry::FeatureParam kMagicStackPushedDown[] = {
    {kMagicStackMostVisitedModuleParam, "false"},
    {kReducedSpaceParam, "-30"}};
const FeatureEntry::FeatureParam kMagicStackReducedNTPTopSpace[] = {
    {kMagicStackMostVisitedModuleParam, "false"},
    {kReducedSpaceParam, "20"}};

const FeatureEntry::FeatureVariation kMagicStackVariations[]{
    {"Most Visited Tiles in Magic Stack", kMagicStackMostVisitedModule,
     std::size(kMagicStackMostVisitedModule), nullptr},
    {"Magic Stack with more NTP Top Space", kMagicStackPushedDown,
     std::size(kMagicStackPushedDown), nullptr},
    {"Magic Stack with Reduced NTP Top Space", kMagicStackReducedNTPTopSpace,
     std::size(kMagicStackReducedNTPTopSpace), nullptr},
};

const FeatureEntry::FeatureParam kHideAllContentSuggestionsTilesAll[] = {
    {kHideContentSuggestionsTilesParamMostVisited, "true"},
    {kHideContentSuggestionsTilesParamShortcuts, "true"},
};
const FeatureEntry::FeatureParam kHideAllContentSuggestionsTilesMVT[] = {
    {kHideContentSuggestionsTilesParamMostVisited, "true"},
    {kHideContentSuggestionsTilesParamShortcuts, "false"},
};
const FeatureEntry::FeatureParam kHideAllContentSuggestionsTilesShortcuts[] = {
    {kHideContentSuggestionsTilesParamMostVisited, "false"},
    {kHideContentSuggestionsTilesParamShortcuts, "true"},
};

const FeatureEntry::FeatureVariation kHideContentSuggestionTilesVariations[]{
    {"Hide all tiles", kHideAllContentSuggestionsTilesAll,
     std::size(kHideAllContentSuggestionsTilesAll), nullptr},
    {"Hide Most Visited tiles", kHideAllContentSuggestionsTilesMVT,
     std::size(kHideAllContentSuggestionsTilesMVT), nullptr},
    {"Hide Shortcuts tiles", kHideAllContentSuggestionsTilesShortcuts,
     std::size(kHideAllContentSuggestionsTilesShortcuts), nullptr},
};

#if BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
// Feed Background Refresh Feature Params.
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

// Feed Background Refresh Feature Variations.
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

// Feed Foreground Refresh Feature Params.
const FeatureEntry::FeatureParam kFeedSessionCloseForegroundRefresh[] = {
    {kEnableFeedSessionCloseForegroundRefresh, "true"},
    {kEnableFeedAppCloseForegroundRefresh, "false"},
    {kEnableFeedAppCloseBackgroundRefresh, "false"}};
const FeatureEntry::FeatureParam kFeedAppCloseForegroundRefresh[] = {
    {kEnableFeedSessionCloseForegroundRefresh, "false"},
    {kEnableFeedAppCloseForegroundRefresh, "true"},
    {kEnableFeedAppCloseBackgroundRefresh, "false"}};
const FeatureEntry::FeatureParam kFeedAppCloseBackgroundRefresh[] = {
    {kEnableFeedSessionCloseForegroundRefresh, "false"},
    {kEnableFeedAppCloseForegroundRefresh, "false"},
    {kEnableFeedAppCloseBackgroundRefresh, "true"}};

// Feed Invisible Foreground Refresh Feature Variations.
const FeatureEntry::FeatureVariation
    kFeedInvisibleForegroundRefreshVariations[] = {
        {"session close foreground refresh", kFeedSessionCloseForegroundRefresh,
         std::size(kFeedSessionCloseForegroundRefresh), nullptr},
        {"app close foreground refresh", kFeedAppCloseForegroundRefresh,
         std::size(kFeedAppCloseForegroundRefresh), nullptr},
        {"app close background refresh", kFeedAppCloseBackgroundRefresh,
         std::size(kFeedAppCloseBackgroundRefresh), nullptr},
};

const FeatureEntry::FeatureParam kEnablePinnedTabsOverflow[] = {
    {kEnablePinnedTabsOverflowParam, "true"}};

const FeatureEntry::FeatureVariation kEnablePinnedTabsVariations[] = {
    {"+ overflow entry", kEnablePinnedTabsOverflow,
     std::size(kEnablePinnedTabsOverflow), nullptr},
};

const FeatureEntry::FeatureParam kAutofillBrandingIOSUntilInteracted[] = {
    {autofill::features::kAutofillBrandingIOSParamFrequencyTypePhone,
     autofill::features::kAutofillBrandingIOSParamFrequencyTypeUntilInteracted},
    {autofill::features::kAutofillBrandingIOSParamFrequencyTypeTablet,
     autofill::features::
         kAutofillBrandingIOSParamFrequencyTypeUntilInteracted}};
const FeatureEntry::FeatureParam kAutofillBrandingIOSAlwaysShowAndSlideOut[] = {
    {autofill::features::kAutofillBrandingIOSParamFrequencyTypePhone,
     autofill::features::
         kAutofillBrandingIOSParamFrequencyTypeAlwaysShowAndDismiss},
    {autofill::features::kAutofillBrandingIOSParamFrequencyTypeTablet,
     autofill::features::kAutofillBrandingIOSParamFrequencyTypeAlways}};
const FeatureEntry::FeatureParam kAutofillBrandingIOSSlideOutWhenInteracted[] =
    {{autofill::features::kAutofillBrandingIOSParamFrequencyTypePhone,
      autofill::features::
          kAutofillBrandingIOSParamFrequencyTypeDismissWhenInteracted},
     {autofill::features::kAutofillBrandingIOSParamFrequencyTypeTablet,
      autofill::features::kAutofillBrandingIOSParamFrequencyTypeAlways}};
const FeatureEntry::FeatureVariation kAutofillBrandingIOSVariations[] = {
    {"(will not show again after user interacts with keyboard accessories)",
     kAutofillBrandingIOSUntilInteracted,
     std::size(kAutofillBrandingIOSUntilInteracted), nullptr},
    {"(shows and slides out from leading edge every time)",
     kAutofillBrandingIOSAlwaysShowAndSlideOut,
     std::size(kAutofillBrandingIOSAlwaysShowAndSlideOut), nullptr},
    {"(slides out from leading edge after user interacts with keyboard "
     "accessories)",
     kAutofillBrandingIOSSlideOutWhenInteracted,
     std::size(kAutofillBrandingIOSSlideOutWhenInteracted), nullptr}};

const FeatureEntry::FeatureParam kNewTabPageFieldTrialTileAblationHideAll[] = {
    {ntp_tiles::kNewTabPageFieldTrialParam, "1"}};
const FeatureEntry::FeatureParam
    kNewTabPageFieldTrialTileAblationHideMVTOnly[] = {
        {ntp_tiles::kNewTabPageFieldTrialParam, "2"}};
const FeatureEntry::FeatureVariation kNewTabPageFieldTrialVariations[] = {
    {"- Tile ablation, Hide all", kNewTabPageFieldTrialTileAblationHideAll,
     std::size(kNewTabPageFieldTrialTileAblationHideAll), nullptr},
    {"- Tile ablation, Hide MVT only",
     kNewTabPageFieldTrialTileAblationHideMVTOnly,
     std::size(kNewTabPageFieldTrialTileAblationHideMVTOnly), nullptr}};

const FeatureEntry::FeatureParam kEnableExpKitTextClassifierDate[] = {
    {"date", "true"}};
const FeatureEntry::FeatureParam kEnableExpKitTextClassifierAddress[] = {
    {"address", "true"}};
const FeatureEntry::FeatureParam kEnableExpKitTextClassifierPhoneNumber[] = {
    {"phonenumber", "true"}};
const FeatureEntry::FeatureParam kEnableExpKitTextClassifierEmail[] = {
    {"email", "true"}};
const FeatureEntry::FeatureParam kEnableExpKitTextClassifierOneTap[] = {
    {"onetap", "true"}};
const FeatureEntry::FeatureParam kEnableExpKitTextClassifierAll[] = {
    {"date", "true"},
    {"address", "true"},
    {"phonenumber", "true"},
    {"email", "true"}};
const FeatureEntry::FeatureVariation kEnableExpKitTextClassifierVariations[] = {
    {"Enabled for all entities", kEnableExpKitTextClassifierAll,
     std::size(kEnableExpKitTextClassifierAll), nullptr},
    {"Enabled for date", kEnableExpKitTextClassifierDate,
     std::size(kEnableExpKitTextClassifierDate), nullptr},
    {"Enabled for address", kEnableExpKitTextClassifierAddress,
     std::size(kEnableExpKitTextClassifierAddress), nullptr},
    {"Enabled for phonenumber", kEnableExpKitTextClassifierPhoneNumber,
     std::size(kEnableExpKitTextClassifierPhoneNumber), nullptr},
    {"Enabled for email", kEnableExpKitTextClassifierEmail,
     std::size(kEnableExpKitTextClassifierEmail), nullptr},
    {"Enabled for One Tap mode", kEnableExpKitTextClassifierOneTap,
     std::size(kEnableExpKitTextClassifierOneTap), nullptr}};

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

const FeatureEntry::FeatureParam kNativeFindInPageWithChromeFindBar[] = {
    {kNativeFindInPageParameterName, kNativeFindInPageWithChromeFindBarParam}};
const FeatureEntry::FeatureVariation kNativeFindInPageVariations[] = {
    {"With Chrome Find Bar", kNativeFindInPageWithChromeFindBar,
     std::size(kNativeFindInPageWithChromeFindBar), nullptr}};

const FeatureEntry::FeatureParam kTabInactivityThresholdOneWeek[] = {
    {kTabInactivityThresholdParameterName,
     kTabInactivityThresholdOneWeekParam}};
const FeatureEntry::FeatureParam kTabInactivityThresholdTwoWeeks[] = {
    {kTabInactivityThresholdParameterName,
     kTabInactivityThresholdTwoWeeksParam}};
const FeatureEntry::FeatureParam kTabInactivityThresholdThreeWeeks[] = {
    {kTabInactivityThresholdParameterName,
     kTabInactivityThresholdThreeWeeksParam}};
const FeatureEntry::FeatureParam kTabInactivityThresholdOneMinuteDemo[] = {
    {kTabInactivityThresholdParameterName,
     kTabInactivityThresholdOneMinuteDemoParam}};

const FeatureEntry::FeatureVariation kTabInactivityThresholdVariations[] = {
    {"One week", kTabInactivityThresholdOneWeek,
     std::size(kTabInactivityThresholdOneWeek), nullptr},
    {"Two weeks", kTabInactivityThresholdTwoWeeks,
     std::size(kTabInactivityThresholdTwoWeeks), nullptr},
    {"Three weeks", kTabInactivityThresholdThreeWeeks,
     std::size(kTabInactivityThresholdThreeWeeks), nullptr},
    {"One minute [Demo]", kTabInactivityThresholdOneMinuteDemo,
     std::size(kTabInactivityThresholdOneMinuteDemo), nullptr},
};

const FeatureEntry::FeatureParam
    kCredentialProviderExtensionPromoOnPasswordSaved[] = {
        {kCredentialProviderExtensionPromoOnPasswordSavedParam, "true"}};
const FeatureEntry::FeatureParam
    kCredentialProviderExtensionPromoOnPasswordCopied[] = {
        {kCredentialProviderExtensionPromoOnPasswordCopiedParam, "true"}};
const FeatureEntry::FeatureParam
    kCredentialProviderExtensionPromoOnLoginWithAutofill[] = {
        {kCredentialProviderExtensionPromoOnLoginWithAutofillParam, "true"}};
const FeatureEntry::FeatureParam
    kCredentialProviderExtensionPromoOnAllTriggers[] = {
        {kCredentialProviderExtensionPromoOnLoginWithAutofillParam, "true"},
        {kCredentialProviderExtensionPromoOnPasswordCopiedParam, "true"},
        {kCredentialProviderExtensionPromoOnPasswordSavedParam, "true"}};

const FeatureEntry::FeatureVariation
    kCredentialProviderExtensionPromoVariations[] = {
        {"On password saved", kCredentialProviderExtensionPromoOnPasswordSaved,
         std::size(kCredentialProviderExtensionPromoOnPasswordSaved), nullptr},
        {"On password copied",
         kCredentialProviderExtensionPromoOnPasswordCopied,
         std::size(kCredentialProviderExtensionPromoOnPasswordCopied), nullptr},
        {"On successful login with autofill",
         kCredentialProviderExtensionPromoOnLoginWithAutofill,
         std::size(kCredentialProviderExtensionPromoOnLoginWithAutofill),
         nullptr},
        {"On all triggers", kCredentialProviderExtensionPromoOnAllTriggers,
         std::size(kCredentialProviderExtensionPromoOnAllTriggers), nullptr},
};

const FeatureEntry::FeatureParam kIOSEditMenuPartialTranslateNoIncognito[] = {
    {kIOSEditMenuPartialTranslateNoIncognitoParam, "true"}};
const FeatureEntry::FeatureParam kIOSEditMenuPartialTranslateWithIncognito[] = {
    {kIOSEditMenuPartialTranslateNoIncognitoParam, "false"}};
const FeatureEntry::FeatureVariation kIOSEditMenuPartialTranslateVariations[] =
    {{"Disable on incognito", kIOSEditMenuPartialTranslateNoIncognito,
      std::size(kIOSEditMenuPartialTranslateNoIncognito), nullptr},
     {"Enable on incognito", kIOSEditMenuPartialTranslateWithIncognito,
      std::size(kIOSEditMenuPartialTranslateWithIncognito), nullptr}};

const FeatureEntry::FeatureParam kAddToHomeScreenDisableIncognito[] = {
    {kAddToHomeScreenDisableIncognitoParam, "true"}};
const FeatureEntry::FeatureVariation kAddToHomeScreenVariations[] = {
    {"Disable on incognito", kAddToHomeScreenDisableIncognito,
     std::size(kAddToHomeScreenDisableIncognito), nullptr}};

const FeatureEntry::FeatureParam kBringYourOwnTabsIOSBottomMessage[] = {
    {kBringYourOwnTabsIOSParam, "true"}};
const FeatureEntry::FeatureVariation kBringYourOwnTabsIOSVariations[] = {
    {"with bottom message on tab grid", kBringYourOwnTabsIOSBottomMessage,
     std::size(kBringYourOwnTabsIOSBottomMessage), nullptr}};

const FeatureEntry::FeatureParam kIOSEditMenuSearchWithTitleSearchWith[] = {
    {kIOSEditMenuSearchWithTitleParamTitle,
     kIOSEditMenuSearchWithTitleSearchWithParam}};
const FeatureEntry::FeatureParam kIOSEditMenuSearchWithTitleSearch[] = {
    {kIOSEditMenuSearchWithTitleParamTitle,
     kIOSEditMenuSearchWithTitleSearchParam}};
const FeatureEntry::FeatureParam kIOSEditMenuSearchWithTitleWebSearch[] = {
    {kIOSEditMenuSearchWithTitleParamTitle,
     kIOSEditMenuSearchWithTitleWebSearchParam}};
const FeatureEntry::FeatureVariation kIOSEditMenuSearchWithVariations[] = {
    {"Search with DSE", kIOSEditMenuSearchWithTitleSearchWith,
     std::size(kIOSEditMenuSearchWithTitleSearchWith), nullptr},
    {"Search", kIOSEditMenuSearchWithTitleSearch,
     std::size(kIOSEditMenuSearchWithTitleSearch), nullptr},
    {"Web Search", kIOSEditMenuSearchWithTitleWebSearch,
     std::size(kIOSEditMenuSearchWithTitleWebSearch), nullptr},
};

const FeatureEntry::FeatureParam kPostRestoreDefaultBrowserPromoHalfscreen[] = {
    {kPostRestoreDefaultBrowserPromoHalfscreenParam, "true"}};
const FeatureEntry::FeatureParam kPostRestoreDefaultBrowserPromoFullscreen[] = {
    {kPostRestoreDefaultBrowserPromoFullscreenParam, "true"}};
const FeatureEntry::FeatureVariation
    kPostRestoreDefaultBrowserPromoVariations[] = {
        {"with half screen ui", kPostRestoreDefaultBrowserPromoHalfscreen,
         std::size(kPostRestoreDefaultBrowserPromoHalfscreen), nullptr},
        {"with full screen ui", kPostRestoreDefaultBrowserPromoFullscreen,
         std::size(kPostRestoreDefaultBrowserPromoFullscreen), nullptr},
};

const FeatureEntry::Choice kReplaceSyncPromosWithSignInPromosChoices[] = {
    {"Default", "", ""},
    {"Base only", "enable-features", "ReplaceSyncPromosWithSignInPromos"},
    {"Everything (bookmarks, reading list, etc)", "enable-features",
     "ReplaceSyncPromosWithSignInPromos,"
     "SyncEnableContactInfoDataType,"
     "SyncEnableContactInfoDataTypeInTransportMode,"
     "EnablePasswordsAccountStorage,"
     "EnableBookmarksAccountStorage,"
     "EnablePreferencesAccountStorage,"
     "ReadingListEnableDualReadingListModel,"
     "ReadingListEnableSyncTransportModeUponSignIn,"
     "ConsistencyNewAccountInterface,"
     "AutofillAccountProfileStorage,"
     "SyncEnableHistoryDataType"},
};

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
     FEATURE_VALUE_TYPE(
         autofill::features::test::kAutofillShowTypePredictions)},
    {"autofill-account-profiles-storage",
     flag_descriptions::kAutofillAccountProfilesStorageName,
     flag_descriptions::kAutofillAccountProfilesStorageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAccountProfileStorage)},
    {"autofill-account-profiles-union-view",
     flag_descriptions::kAutofillAccountProfilesUnionViewName,
     flag_descriptions::kAutofillAccountProfilesUnionViewDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAccountProfilesUnionView)},
    {"autofill-ios-delay-between-fields",
     flag_descriptions::kAutofillIOSDelayBetweenFieldsName,
     flag_descriptions::kAutofillIOSDelayBetweenFieldsDescription,
     flags_ui::kOsIos, MULTI_VALUE_TYPE(kAutofillIOSDelayBetweenFieldsChoices)},
    {"fullscreen-promos-manager-skip-internal-limits",
     flag_descriptions::kFullscreenPromosManagerSkipInternalLimitsName,
     flag_descriptions::kFullscreenPromosManagerSkipInternalLimitsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenPromosManagerSkipInternalLimits)},
    {"fullscreen-viewport-adjustment-experiment",
     flag_descriptions::kFullscreenSmoothScrollingName,
     flag_descriptions::kFullscreenSmoothScrollingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kSmoothScrollingDefault)},
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

    {"omnibox-most-visited-tiles-on-srp",
     flag_descriptions::kOmniboxMostVisitedTilesOnSrpName,
     flag_descriptions::kOmniboxMostVisitedTilesOnSrpDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxMostVisitedTilesOnSrp)},
    {"autofill-use-mobile-label-disambiguation",
     flag_descriptions::kAutofillUseMobileLabelDisambiguationName,
     flag_descriptions::kAutofillUseMobileLabelDisambiguationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUseMobileLabelDisambiguation,
         kAutofillUseMobileLabelDisambiguationVariations,
         "AutofillUseMobileLabelDisambiguation")},
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kForceStartupSigninPromo)},
    {"restore-session-from-cache",
     flag_descriptions::kRestoreSessionFromCacheName,
     flag_descriptions::kRestoreSessionFromCacheDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kRestoreSessionFromCache)},
    {"expanded-tab-strip", flag_descriptions::kExpandedTabStripName,
     flag_descriptions::kExpandedTabStripDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kExpandedTabStrip)},
    {"shared-highlighting-ios", flag_descriptions::kSharedHighlightingIOSName,
     flag_descriptions::kSharedHighlightingIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSharedHighlightingIOS)},
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
    {"start-surface", flag_descriptions::kStartSurfaceName,
     flag_descriptions::kStartSurfaceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kStartSurface,
                                    kStartSurfaceVariations,
                                    "StartSurface")},
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
    {"incognito-ntp-revamp", flag_descriptions::kIncognitoNtpRevampName,
     flag_descriptions::kIncognitoNtpRevampDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIncognitoNtpRevamp)},
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
    {"overflow-menu-customization",
     flag_descriptions::kOverflowMenuCustomizationName,
     flag_descriptions::kOverflowMenuCustomizationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOverflowMenuCustomization)},
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
    {"enable-lens-context-menu-alt-text",
     flag_descriptions::kEnableLensContextMenuAltTextName,
     flag_descriptions::kEnableLensContextMenuAltTextDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableLensContextMenuAltText)},
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
    {"feed-header-settings", flag_descriptions::kEnableFeedHeaderSettingsName,
     flag_descriptions::kEnableFeedHeaderSettingsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kFeedHeaderSettings,
                                    kFeedHeaderSettingsVariations,
                                    "FeedHeaderSettings")},
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
    {"optimization-guide-install-wide-model-store",
     flag_descriptions::kOptimizationGuideInstallWideModelStoreName,
     flag_descriptions::kOptimizationGuideInstallWideModelStoreDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(optimization_guide::features::
                            kOptimizationGuideInstallWideModelStore)},
    {"optimization-guide-push-notifications",
     flag_descriptions::kOptimizationGuidePushNotificationClientName,
     flag_descriptions::kOptimizationGuidePushNotificationClientDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(optimization_guide::features::kPushNotifications)},
    {"sync-segments-data", flag_descriptions::kSyncSegmentsDataName,
     flag_descriptions::kSyncSegmentsDataDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(history::kSyncSegmentsData)},
    {"suggestions-scrolling-ipad",
     flag_descriptions::kEnableSuggestionsScrollingOnIPadName,
     flag_descriptions::kEnableSuggestionsScrollingOnIPadDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableSuggestionsScrollingOnIPad)},
    {"popout-omnibox-ipad", flag_descriptions::kEnablePopoutOmniboxIpadName,
     flag_descriptions::kEnablePopoutOmniboxIpadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnablePopoutOmniboxIpad)},
    {"intents-on-phone-number", flag_descriptions::kPhoneNumberName,
     flag_descriptions::kPhoneNumberDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnablePhoneNumbers)},
    {"experience-kit-apple-calendar",
     flag_descriptions::kAppleCalendarExperienceKitName,
     flag_descriptions::kAppleCalendarExperienceKitDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableExpKitAppleCalendar)},
    {"enable-expkit-text-classifier",
     flag_descriptions::kEnableExpKitTextClassifierName,
     flag_descriptions::kEnableExpKitTextClassifierDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableExpKitTextClassifier,
                                    kEnableExpKitTextClassifierVariations,
                                    "ExpKitTextClassifier")},
    {"one-tap-experience-maps", flag_descriptions::kOneTapForMapsName,
     flag_descriptions::kOneTapForMapsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kOneTapForMaps)},
    {"https-only-mode", flag_descriptions::kHttpsOnlyModeName,
     flag_descriptions::kHttpsOnlyModeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(security_interstitials::features::kHttpsOnlyMode)},
    {"omnibox-https-upgrades", flag_descriptions::kOmniboxHttpsUpgradesName,
     flag_descriptions::kOmniboxHttpsUpgradesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kDefaultTypedNavigationsToHttps)},
    {"smart-sorting-price-tracking-destination",
     flag_descriptions::kSmartSortingPriceTrackingDestinationName,
     flag_descriptions::kSmartSortingPriceTrackingDestinationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSmartSortingPriceTrackingDestination)},
    {"new-overflow-menu-share-chrome-action",
     flag_descriptions::kNewOverflowMenuShareChromeActionName,
     flag_descriptions::kNewOverflowMenuShareChromeActionDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNewOverflowMenuShareChromeAction)},
    {"autofill-enable-ranking-formula-address-profiles",
     flag_descriptions::kAutofillEnableRankingFormulaAddressProfilesName,
     flag_descriptions::kAutofillEnableRankingFormulaAddressProfilesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableRankingFormulaAddressProfiles)},
    {"autofill-enable-ranking-formula-credit-cards",
     flag_descriptions::kAutofillEnableRankingFormulaCreditCardsName,
     flag_descriptions::kAutofillEnableRankingFormulaCreditCardsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableRankingFormulaCreditCards)},
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
    {"content-suggestions-magic-stack", flag_descriptions::kMagicStackName,
     flag_descriptions::kMagicStackDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kMagicStack,
                                    kMagicStackVariations,
                                    flag_descriptions::kMagicStackName)},
    {"default-browser-intents-show-settings",
     flag_descriptions::kDefaultBrowserIntentsShowSettingsName,
     flag_descriptions::kDefaultBrowserIntentsShowSettingsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDefaultBrowserIntentsShowSettings)},
    {"ios-password-ui-split", flag_descriptions::kIOSPasswordUISplitName,
     flag_descriptions::kIOSPasswordUISplitDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSPasswordUISplit)},
    {"ios-set-up-list", flag_descriptions::kIOSSetUpListName,
     flag_descriptions::kIOSSetUpListDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSSetUpList)},
    {"ios-password-bottom-sheet",
     flag_descriptions::kIOSPasswordBottomSheetName,
     flag_descriptions::kIOSPasswordBottomSheetDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSPasswordBottomSheet)},
    {"ios-payments-bottom-sheet",
     flag_descriptions::kIOSPaymentsBottomSheetName,
     flag_descriptions::kIOSPaymentsBottomSheetDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSPaymentsBottomSheet)},
    {"ios-new-tab-page-retention", flag_descriptions::kNewTabPageFieldTrialName,
     flag_descriptions::kNewTabPageFieldTrialDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(ntp_tiles::kNewTabPageFieldTrial,
                                    kNewTabPageFieldTrialVariations,
                                    ntp_tiles::kNewTabPageFieldTrialName)},
    {"autofill-parse-iban-fields",
     flag_descriptions::kAutofillParseIBANFieldsName,
     flag_descriptions::kAutofillParseIBANFieldsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillParseIBANFields)},
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
    {"enable-browser-lockdown-mode",
     flag_descriptions::kBrowserLockdownModeAvailableName,
     flag_descriptions::kBrowserLockdownModeAvailableDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(web::kBrowserLockdownModeAvailable)},
    {"enable-button-configuration-usage",
     flag_descriptions::kEnableUIButtonConfigurationName,
     flag_descriptions::kEnableUIButtonConfigurationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableUIButtonConfiguration)},
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
    {"default-browser-refactoring-promo-manager",
     flag_descriptions::kDefaultBrowserRefactoringPromoManagerName,
     flag_descriptions::kDefaultBrowserRefactoringPromoManagerDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDefaultBrowserRefactoringPromoManager)},
    {"default-browser-video-promo",
     flag_descriptions::kDefaultBrowserVideoPromoName,
     flag_descriptions::kDefaultBrowserVideoPromoDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDefaultBrowserVideoPromo,
                                    kDefaultBrowserVideoPromoVariations,
                                    "DefaultBrowserVideoPromoVariations")},
    {"default-browser-promo-force-show-promo",
     flag_descriptions::kDefaultBrowserPromoForceShowPromoName,
     flag_descriptions::kDefaultBrowserPromoForceShowPromoDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kDefaultBrowserPromoForceShowPromoChoices)},
    {"default-browser-promo-trigger-criteria-experiment",
     flag_descriptions::kDefaultBrowserTriggerCriteriaExperimentName,
     flag_descriptions::kDefaultBrowserTriggerCriteriaExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kDefaultBrowserTriggerCriteriaExperiment,
         kDefaultBrowserTriggerCriteriaExperimentParams,
         "DefaultBrowserTriggerCriteriaExperimentParams")},
#if BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"feed-background-refresh-ios",
     flag_descriptions::kFeedBackgroundRefreshName,
     flag_descriptions::kFeedBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFeedBackgroundRefresh,
                                    kFeedBackgroundRefreshVariations,
                                    "FeedBackgroundRefresh")},
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"feed-invisible-foreground-refresh-ios",
     flag_descriptions::kFeedInvisibleForegroundRefreshName,
     flag_descriptions::kFeedInvisibleForegroundRefreshDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFeedInvisibleForegroundRefresh,
                                    kFeedInvisibleForegroundRefreshVariations,
                                    "FeedInvisibleForegroundRefresh")},
    {"omnibox-keyboard-paste-button",
     flag_descriptions::kOmniboxKeyboardPasteButtonName,
     flag_descriptions::kOmniboxKeyboardPasteButtonDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kOmniboxKeyboardPasteButton)},
    {"whats-new-ios-m116", flag_descriptions::kWhatsNewIOSM116Name,
     flag_descriptions::kWhatsNewIOSM116Description, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kWhatsNewIOSM116)},
    {"ios-autofill-branding", flag_descriptions::kAutofillBrandingIOSName,
     flag_descriptions::kAutofillBrandingIOSDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(autofill::features::kAutofillBrandingIOS,
                                    kAutofillBrandingIOSVariations,
                                    "AutofillBrandingIOS")},
    {"app-store-rating", flag_descriptions::kAppStoreRatingName,
     flag_descriptions::kAppStoreRatingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAppStoreRating)},
    {"most-visited-tiles", flag_descriptions::kMostVisitedTilesName,
     flag_descriptions::kMostVisitedTilesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kMostVisitedTiles)},
    {"enable-tflite-language-detection-ignore",
     flag_descriptions::kTFLiteLanguageDetectionIgnoreName,
     flag_descriptions::kTFLiteLanguageDetectionIgnoreDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(translate::kTFLiteLanguageDetectionIgnoreEnabled)},
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
    {"tab-grid-recency-sort", flag_descriptions::kTabGridRecencySortName,
     flag_descriptions::kTabGridRecencySortDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridRecencySort)},
    {"tab-grid-new-transitions", flag_descriptions::kTabGridNewTransitionsName,
     flag_descriptions::kTabGridNewTransitionsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridNewTransitions)},
    {"enable-pinned-tabs", flag_descriptions::kEnablePinnedTabsName,
     flag_descriptions::kEnablePinnedTabsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnablePinnedTabs,
                                    kEnablePinnedTabsVariations,
                                    "EnablePinnedTabs")},
    {"credential-provider-extension-promo",
     flag_descriptions::kCredentialProviderExtensionPromoName,
     flag_descriptions::kCredentialProviderExtensionPromoDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kCredentialProviderExtensionPromo,
                                    kCredentialProviderExtensionPromoVariations,
                                    "CredentialProviderExtensionPromo")},
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
    {"autofill-offer-to-save-card-with-same-last-four",
     flag_descriptions::kAutofillOfferToSaveCardWithSameLastFourName,
     flag_descriptions::kAutofillOfferToSaveCardWithSameLastFourDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillOfferToSaveCardWithSameLastFour)},
    {"omnibox-multiline-search-suggest",
     flag_descriptions::kOmniboxMultilineSearchSuggestName,
     flag_descriptions::kOmniboxMultilineSearchSuggestDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kOmniboxMultilineSearchSuggest)},
    {"autofill-suggest-server-card-instead-of-local-card",
     flag_descriptions::kAutofillSuggestServerCardInsteadOfLocalCardName,
     flag_descriptions::kAutofillSuggestServerCardInsteadOfLocalCardDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSuggestServerCardInsteadOfLocalCard)},
    {"native-find-in-page", flag_descriptions::kNativeFindInPageName,
     flag_descriptions::kNativeFindInPageDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kNativeFindInPage,
                                    kNativeFindInPageVariations,
                                    "NativeFindInPage")},
    {"intents-on-email", flag_descriptions::kEmailName,
     flag_descriptions::kEmailDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableEmails)},
    {"ios-password-checkup", flag_descriptions::kIOSPasswordCheckupName,
     flag_descriptions::kIOSPasswordCheckupDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSPasswordCheckup)},
    {"multiline-fade-truncating-label",
     flag_descriptions::kMultilineFadeTruncatingLabelName,
     flag_descriptions::kMultilineFadeTruncatingLabelDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kMultilineFadeTruncatingLabel)},
    {"promos-manager-uses-fet", flag_descriptions::kPromosManagerUsesFETName,
     flag_descriptions::kPromosManagerUsesFETDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPromosManagerUsesFET)},
    {"shopping-list", commerce::flag_descriptions::kShoppingListName,
     commerce::flag_descriptions::kShoppingListDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kShoppingList)},
    {"shopping-list-track-by-default",
     commerce::flag_descriptions::kShoppingListTrackByDefaultName,
     commerce::flag_descriptions::kShoppingListTrackByDefaultDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kShoppingListTrackByDefault)},
    {"local-pdp-detection",
     commerce::flag_descriptions::kCommerceLocalPDPDetectionName,
     commerce::flag_descriptions::kCommerceLocalPDPDetectionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kCommerceLocalPDPDetection)},
    {"tab-inactivity-threshold", flag_descriptions::kTabInactivityThresholdName,
     flag_descriptions::kTabInactivityThresholdDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kTabInactivityThreshold,
                                    kTabInactivityThresholdVariations,
                                    "TabInactivityThreshold")},
    {"enable-feed-synthetic-capabilities",
     flag_descriptions::kEnableFeedSyntheticCapabilitiesName,
     flag_descriptions::kEnableFeedSyntheticCapabilitiesDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableFeedSyntheticCapabilities)},
    {"show-inactive-tabs-count", flag_descriptions::kShowInactiveTabsCountName,
     flag_descriptions::kShowInactiveTabsCountDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kShowInactiveTabsCount)},
    {"ios-edit-menu-partial-translate",
     flag_descriptions::kIOSEditMenuPartialTranslateName,
     flag_descriptions::kIOSEditMenuPartialTranslateDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSEditMenuPartialTranslate,
                                    kIOSEditMenuPartialTranslateVariations,
                                    "IOSEditMenuPartialTranslate")},
    {"ios-custom-browser-edit-menu",
     flag_descriptions::kIOSCustomBrowserEditMenuName,
     flag_descriptions::kIOSCustomBrowserEditMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSCustomBrowserEditMenu)},
    {"notification-settings-menu-item",
     flag_descriptions::kNotificationSettingsMenuItemName,
     flag_descriptions::kNotificationSettingsMenuItemDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNotificationSettingsMenuItem)},
    {"password-notes", flag_descriptions::kPasswordNotesWithBackupName,
     flag_descriptions::kPasswordNotesWithBackupDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kPasswordNotesWithBackup)},
    {"feed-experiment-tagging-ios",
     flag_descriptions::kFeedExperimentTaggingName,
     flag_descriptions::kFeedExperimentTaggingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFeedExperimentTagging)},
    {"spotlight-reading-list-source",
     flag_descriptions::kSpotlightReadingListSourceName,
     flag_descriptions::kSpotlightReadingListSourceDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kSpotlightReadingListSource)},
    {"consistency-new-account-interface",
     flag_descriptions::kConsistencyNewAccountInterfaceName,
     flag_descriptions::kConsistencyNewAccountInterfaceDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kConsistencyNewAccountInterface)},
    {"add-to-home-screen", flag_descriptions::kAddToHomeScreenName,
     flag_descriptions::kAddToHomeScreenDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kAddToHomeScreen,
                                    kAddToHomeScreenVariations,
                                    "IOSEditMenuPartialTranslate")},
    {"policy-logs-page-ios", flag_descriptions::kPolicyLogsPageIOSName,
     flag_descriptions::kPolicyLogsPageIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(policy::features::kPolicyLogsPageIOS)},
    {"indicate-account-storage-error-in-account-cell",
     flag_descriptions::kIndicateAccountStorageErrorInAccountCellName,
     flag_descriptions::kIndicateAccountStorageErrorInAccountCellDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kIndicateAccountStorageErrorInAccountCell)},
    {"enable-bookmarks-account-storage",
     flag_descriptions::kEnableBookmarksAccountStorageName,
     flag_descriptions::kEnableBookmarksAccountStorageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(bookmarks::kEnableBookmarksAccountStorage)},
    {"web-feed-feedback-reroute",
     flag_descriptions::kWebFeedFeedbackRerouteName,
     flag_descriptions::kWebFeedFeedbackRerouteDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kWebFeedFeedbackReroute)},
    {"new-ntp-omnibox-layout", flag_descriptions::kNewNTPOmniboxLayoutName,
     flag_descriptions::kNewNTPOmniboxLayoutDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewNTPOmniboxLayout)},
    {"bring-your-own-tabs-ios", flag_descriptions::kBringYourOwnTabsIOSName,
     flag_descriptions::kBringYourOwnTabsIOSDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBringYourOwnTabsIOS,
                                    kBringYourOwnTabsIOSVariations,
                                    "BringYourOwnTabsIOS")},
    {"enable-follow-IPH-exp-params",
     flag_descriptions::kEnableFollowIPHExpParamsName,
     flag_descriptions::kEnableFollowIPHExpParamsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFollowIPHExpParams)},
    {"enable-follow-management-instant-reload",
     flag_descriptions::kEnableFollowManagementInstantReloadName,
     flag_descriptions::kEnableFollowManagementInstantReloadDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFollowManagementInstantReload)},
    {"enable-follow-ui-update", flag_descriptions::kEnableFollowUIUpdateName,
     flag_descriptions::kEnableFollowUIUpdateDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFollowUIUpdate)},
    {"mixed-content-autoupgrade-ios",
     flag_descriptions::kMixedContentAutoupgradeName,
     flag_descriptions::kMixedContentAutoupgradeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         security_interstitials::features::kMixedContentAutoupgrade)},
    {"enable-email-in-bookmarks-reading-list-snackbar",
     flag_descriptions::kEnableEmailInBookmarksReadingListSnackbarName,
     flag_descriptions::kEnableEmailInBookmarksReadingListSnackbarDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableEmailInBookmarksReadingListSnackbar)},
    {"autofill-upstream-authenticate-preflight-call",
     flag_descriptions::kAutofillUpstreamAuthenticatePreflightCallName,
     flag_descriptions::kAutofillUpstreamAuthenticatePreflightCallDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamAuthenticatePreflightCall)},
    {"enable-preferences-account-storage",
     flag_descriptions::kEnablePreferencesAccountStorageName,
     flag_descriptions::kEnablePreferencesAccountStorageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kEnablePreferencesAccountStorage)},
    {"indicate-identity-error-overflow-menu",
     flag_descriptions::kIndicateIdentityErrorInOverflowMenuName,
     flag_descriptions::kIndicateIdentityErrorInOverflowMenuDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIndicateSyncErrorInOverflowMenu)},
    {"ios-browser-edit-menu-metrics",
     flag_descriptions::kIOSBrowserEditMenuMetricsName,
     flag_descriptions::kIOSBrowserEditMenuMetricsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSBrowserEditMenuMetrics)},
    {"sf-symbols-follow-up", flag_descriptions::kSFSymbolsFollowUpName,
     flag_descriptions::kSFSymbolsFollowUpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSFSymbolsFollowUp)},
    {"enable-reading-list-account-storage",
     flag_descriptions::kEnableReadingListAccountStorageName,
     flag_descriptions::kEnableReadingListAccountStorageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         reading_list::switches::kReadingListEnableDualReadingListModel)},
    {"enable-reading-list-sign-in-promo",
     flag_descriptions::kEnableReadingListSignInPromoName,
     flag_descriptions::kEnableReadingListSignInPromoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(reading_list::switches::
                            kReadingListEnableSyncTransportModeUponSignIn)},
    {"omnibox-grouping-framework-zps",
     flag_descriptions::kOmniboxGroupingFrameworkForZPSName,
     flag_descriptions::kOmniboxGroupingFrameworkForZPSDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kGroupingFrameworkForZPS)},
    {"omnibox-grouping-framework-non-zps",
     flag_descriptions::kOmniboxGroupingFrameworkForTypedSuggestionsName,
     flag_descriptions::kOmniboxGroupingFrameworkForTypedSuggestionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kGroupingFrameworkForNonZPS)},
    {"enable-session-serialization-optimizations",
     flag_descriptions::kEnableSessionSerializationOptimizationsName,
     flag_descriptions::kEnableSessionSerializationOptimizationsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         web::features::kEnableSessionSerializationOptimizations)},
    {"bottom-omnibox-steady-state",
     flag_descriptions::kBottomOmniboxSteadyStateName,
     flag_descriptions::kBottomOmniboxSteadyStateDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kBottomOmniboxSteadyState)},
    {"autofill-upstream-use-alternate-secure-data-type",
     flag_descriptions::kAutofillUpstreamUseAlternateSecureDataTypeName,
     flag_descriptions::kAutofillUpstreamUseAlternateSecureDataTypeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamUseAlternateSecureDataType)},
    {"only-access-clipboard-async",
     flag_descriptions::kOnlyAccessClipboardAsyncName,
     flag_descriptions::kOnlyAccessClipboardAsyncDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOnlyAccessClipboardAsync)},
    {"omnibox-tail-suggest", flag_descriptions::kOmniboxTailSuggestName,
     flag_descriptions::kOmniboxTailSuggestDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOmniboxTailSuggest)},
    {"feed-disable-hot-start-refresh-ios",
     flag_descriptions::kFeedDisableHotStartRefreshName,
     flag_descriptions::kFeedDisableHotStartRefreshDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kFeedDisableHotStartRefresh)},
    {"ios-edit-menu-search-with", flag_descriptions::kIOSEditMenuSearchWithName,
     flag_descriptions::kIOSEditMenuSearchWithDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSEditMenuSearchWith,
                                    kIOSEditMenuSearchWithVariations,
                                    "IOSEditMenuSearchWith")},
    {"ios-edit-menu-hide-search-web",
     flag_descriptions::kIOSEditMenuHideSearchWebName,
     flag_descriptions::kIOSEditMenuHideSearchWebDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSEditMenuHideSearchWeb)},
    {"autofill-enable-card-art-image",
     flag_descriptions::kAutofillEnableCardArtImageName,
     flag_descriptions::kAutofillEnableCardArtImageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardArtImage)},
    {"hide-content-suggestions-tiles",
     flag_descriptions::kHideContentSuggestionTilesName,
     flag_descriptions::kHideContentSuggestionTilesDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kHideContentSuggestionsTiles,
         kHideContentSuggestionTilesVariations,
         flag_descriptions::kHideContentSuggestionTilesName)},
    {"enable-signed-out-view-demotion",
     flag_descriptions::kEnableSignedOutViewDemotionName,
     flag_descriptions::kEnableSignedOutViewDemotionDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableSignedOutViewDemotion)},
    {"autofill-use-two-dots-for-last-four-digits",
     flag_descriptions::kAutofillUseTwoDotsForLastFourDigitsName,
     flag_descriptions::kAutofillUseTwoDotsForLastFourDigitsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUseTwoDotsForLastFourDigits)},
    {"enable-variations-google-group-filtering",
     flag_descriptions::kEnableVariationsGoogleGroupFilteringName,
     flag_descriptions::kEnableVariationsGoogleGroupFilteringDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kVariationsGoogleGroupFiltering)},
    {"autofill-disable-profile-updates",
     flag_descriptions::kAutofillDisableProfileUpdatesName,
     flag_descriptions::kAutofillDisableProfileUpdatesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::test::kAutofillDisableProfileUpdates)},
    {"autofill-disable-silent-profile-updates",
     flag_descriptions::kAutofillDisableSilentProfileUpdatesName,
     flag_descriptions::kAutofillDisableSilentProfileUpdatesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::test::kAutofillDisableSilentProfileUpdates)},
    {"autofill-enable-admin-level-2",
     flag_descriptions::kAutofillEnableSupportForAdminLevel2Name,
     flag_descriptions::kAutofillEnableSupportForAdminLevel2Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSupportForAdminLevel2)},
    {"autofill-enable-between-streets",
     flag_descriptions::kAutofillEnableSupportForBetweenStreetsName,
     flag_descriptions::kAutofillEnableSupportForBetweenStreetsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSupportForBetweenStreets)},
    {"autofill-enable-landmark",
     flag_descriptions::kAutofillEnableSupportForLandmarkName,
     flag_descriptions::kAutofillEnableSupportForLandmarkDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableSupportForLandmark)},
    {"post-restore-default-browser-promo",
     flag_descriptions::kPostRestoreDefaultBrowserPromoName,
     flag_descriptions::kPostRestoreDefaultBrowserPromoDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kPostRestoreDefaultBrowserPromo,
         kPostRestoreDefaultBrowserPromoVariations,
         "PostRestoreDefaultBrowserPromoVariations")},
    {"spotlight-open-tabs-source",
     flag_descriptions::kSpotlightOpenTabsSourceName,
     flag_descriptions::kSpotlightOpenTabsSourceDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSpotlightOpenTabsSource)},
    {"replace-sync-promos-with-sign-in-promos",
     flag_descriptions::kReplaceSyncPromosWithSignInPromosName,
     flag_descriptions::kReplaceSyncPromosWithSignInPromosDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kReplaceSyncPromosWithSignInPromosChoices)},
    {"toolbar-theme-color", flag_descriptions::kThemeColorInToolbarName,
     flag_descriptions::kThemeColorInToolbarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kThemeColorInToolbar)},
    {"tab-grid-refactoring", flag_descriptions::kTabGridRefactoringName,
     flag_descriptions::kTabGridRefactoringDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridRefactoring)},
    {"ios-iph-for-safari-switcher",
     flag_descriptions::kIPHForSafariSwitcherName,
     flag_descriptions::kIPHForSafariSwitcherDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIPHForSafariSwitcher)},
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

  if ([defaults boolForKey:@"DisablePasswordManagerPolicy"]) {
    NSString* password_manager_key =
        base::SysUTF8ToNSString(policy::key::kPasswordManagerEnabled);
    [testing_policies addEntriesFromDictionary:@{password_manager_key : @NO}];
    [allowed_experimental_policies addObject:password_manager_key];
  }
  if ([defaults boolForKey:@"AddManagedBookmarks"]) {
    NSString* managed_bookmarks_key =
        base::SysUTF8ToNSString(policy::key::kManagedBookmarks);
    NSString* managed_bookmarks_value =
        @"["
         // The following gets filtered out from
         // the JSON string when parsed.
         "  {"
         "    \"toplevel_name\": \"Managed Bookmarks\""
         "  },"
         "  {"
         "    \"name\": \"Google\","
         "    \"url\": \"google.com\""
         "  },"
         "  {"
         "    \"name\": \"Empty Folder\","
         "    \"children\": []"
         "  },"
         "  {"
         "    \"name\": \"Big Folder\","
         "    \"children\": ["
         "      {"
         "        \"name\": \"Youtube\","
         "        \"url\": \"youtube.com\""
         "      },"
         "      {"
         "        \"name\": \"Chromium\","
         "        \"url\": \"chromium.org\""
         "      },"
         "      {"
         "        \"name\": \"More Stuff\","
         "        \"children\": ["
         "          {"
         "            \"name\": \"Bugs\","
         "            \"url\": \"crbug.com\""
         "          }"
         "        ]"
         "      }"
         "    ]"
         "  }"
         "]";
    [testing_policies addEntriesFromDictionary:@{
      managed_bookmarks_key : managed_bookmarks_value
    }];
    [allowed_experimental_policies addObject:managed_bookmarks_key];
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
      [testing_policies setValue:@YES forKey:metrics_reporting_key];
      break;
    case 2:
      // Metrics reporting disabled.
      [testing_policies setValue:@NO forKey:metrics_reporting_key];
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

bool IsRestartNeededToCommitChanges() {
  return GetGlobalFlagsState().IsRestartNeededToCommitChanges();
}

namespace testing {

base::span<const flags_ui::FeatureEntry> GetFeatureEntries() {
  return base::span<const flags_ui::FeatureEntry>(kFeatureEntries,
                                                  std::size(kFeatureEntries));
}

}  // namespace testing
