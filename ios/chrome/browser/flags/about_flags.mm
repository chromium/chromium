// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of about_flags for iOS that sets flags based on experimental
// settings.

#import "ios/chrome/browser/flags/about_flags.h"

#import <UIKit/UIKit.h>
#import <stddef.h>
#import <stdint.h>

#import "base/apple/foundation_util.h"
#import "base/base_switches.h"
#import "base/check_op.h"
#import "base/debug/debugging_buildflags.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/core/common/autofill_switches.h"
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
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/payments/core/features.h"
#import "components/policy/core/common/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/reading_list/features/reading_list_switches.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/send_tab_to_self/features.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "components/signin/core/browser/account_reconcilor.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "components/supervised_user/core/common/features.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/features.h"
#import "components/sync/base/pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_features.h"
#import "ios/chrome/browser/crash_report/model/features.h"
#import "ios/chrome/browser/credential_provider_promo/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/follow/model/follow_features.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/features.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/screen_time/model/screen_time_buildflags.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/features.h"
#import "ios/chrome/browser/text_selection/model/text_selection_util.h"
#import "ios/chrome/browser/ui/app_store_rating/features.h"
#import "ios/chrome/browser/ui/default_promo/post_restore/features.h"
#import "ios/chrome/browser/ui/download/features.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/features.h"
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
#import "ios/chrome/browser/screen_time/model/features.h"
#endif

#if !defined(OFFICIAL_BUILD)
#import "components/variations/variations_switches.h"
#endif

using flags_ui::FeatureEntry;

namespace {

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

const FeatureEntry::FeatureParam
    kOmniboxCompanyEntityAdjustmentLeastAggressive[] = {
        {"OmniboxCompanyEntityAdjustmentGroup", "least-aggressive"}};
const FeatureEntry::FeatureParam kOmniboxCompanyEntityAdjustmentModerate[] = {
    {"OmniboxCompanyEntityAdjustmentGroup", "moderate"}};
const FeatureEntry::FeatureParam
    kOmniboxCompanyEntityAdjustmentMostAggressive[] = {
        {"OmniboxCompanyEntityAdjustmentGroup", "most-aggressive"}};

const FeatureEntry::FeatureVariation
    kOmniboxCompanyEntityAdjustmentVariations[] = {
        {"Least Aggressive", kOmniboxCompanyEntityAdjustmentLeastAggressive,
         std::size(kOmniboxCompanyEntityAdjustmentLeastAggressive), nullptr},
        {"Moderate", kOmniboxCompanyEntityAdjustmentModerate,
         std::size(kOmniboxCompanyEntityAdjustmentModerate), nullptr},
        {"Most Aggressive", kOmniboxCompanyEntityAdjustmentMostAggressive,
         std::size(kOmniboxCompanyEntityAdjustmentMostAggressive), nullptr},
};

const FeatureEntry::FeatureParam
    kDefaultBrowserVideoConditionsHalfscreenPromo[] = {
        {kDefaultBrowserVideoPromoVariant, kVideoConditionsHalfscreenPromo}};
const FeatureEntry::FeatureParam
    kDefaultBrowserVideoConditionsFullscreenPromo[] = {
        {kDefaultBrowserVideoPromoVariant, kVideoConditionsFullscreenPromo}};
const FeatureEntry::FeatureParam
    kDefaultBrowserGenericConsitionsFullscreenPromo[] = {
        {kDefaultBrowserVideoPromoVariant, kGenericConditionsFullscreenPromo}};
const FeatureEntry::FeatureParam
    kDefaultBrowserGenericConditionsHalfscreenPromo[] = {
        {kDefaultBrowserVideoPromoVariant, kGenericConditionsHalfscreenPromo}};

const FeatureEntry::FeatureVariation kDefaultBrowserVideoPromoVariations[] = {
    {"Show half screen ui with video condtions",
     kDefaultBrowserVideoConditionsHalfscreenPromo,
     std::size(kDefaultBrowserVideoConditionsHalfscreenPromo), nullptr},
    {"Show full screen ui with video condtions",
     kDefaultBrowserVideoConditionsFullscreenPromo,
     std::size(kDefaultBrowserVideoConditionsFullscreenPromo), nullptr},
    {"Show full screen ui with generic condtions",
     kDefaultBrowserGenericConsitionsFullscreenPromo,
     std::size(kDefaultBrowserGenericConsitionsFullscreenPromo), nullptr},
    {"Show half screen ui with generic condtions",
     kDefaultBrowserGenericConditionsHalfscreenPromo,
     std::size(kDefaultBrowserGenericConditionsHalfscreenPromo), nullptr},
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
const FeatureEntry::FeatureParam kDiscoverFeedTopSyncPromoCompactHorizontal[] =
    {{kDiscoverFeedTopSyncPromoStyle, "1"}};
const FeatureEntry::FeatureParam kDiscoverFeedTopSyncPromoCompactVertical[] = {
    {kDiscoverFeedTopSyncPromoStyle, "2"}};

const FeatureEntry::FeatureVariation kDiscoverFeedTopSyncPromoVariations[] = {
    {"Standard", kDiscoverFeedTopSyncPromoStandard,
     std::size(kDiscoverFeedTopSyncPromoStandard), nullptr},
    {"Compact Horizontal", kDiscoverFeedTopSyncPromoCompactHorizontal,
     std::size(kDiscoverFeedTopSyncPromoCompactHorizontal), nullptr},
    {"Compact Vertical", kDiscoverFeedTopSyncPromoCompactVertical,
     std::size(kDiscoverFeedTopSyncPromoCompactVertical), nullptr}};

const FeatureEntry::FeatureParam kContentPushNotificationsEnabledPromo[] = {
    {kContentPushNotificationsExperimentType, "1"}};
const FeatureEntry::FeatureParam kContentPushNotificationsEnabledSetupLists[] =
    {{kContentPushNotificationsExperimentType, "2"}};

const FeatureEntry::FeatureVariation kContentPushNotificationsVariations[] = {
    {"Promo", kContentPushNotificationsEnabledPromo,
     std::size(kContentPushNotificationsEnabledPromo), nullptr},
    {"Set up list", kContentPushNotificationsEnabledSetupLists,
     std::size(kContentPushNotificationsEnabledSetupLists), nullptr}};

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

const FeatureEntry::FeatureParam kStartSurfaceTenSeconds[] = {
    {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam kStartSurfaceOneHour[] = {
    {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};

const FeatureEntry::FeatureVariation kStartSurfaceVariations[] = {
    {"10s:Show Home Surface", kStartSurfaceTenSeconds,
     std::size(kStartSurfaceTenSeconds), nullptr},
    {"1h:Show Home Surface", kStartSurfaceOneHour,
     std::size(kStartSurfaceOneHour), nullptr},
};

const FeatureEntry::FeatureParam kMagicStackMostVisitedModule[] = {
    {kMagicStackMostVisitedModuleParam, "true"},
    {kReducedSpaceParam, "-80"}};
const FeatureEntry::FeatureParam
    kMagicStackMostVisitedModuleHideIrrelevantModules[] = {
        {kMagicStackMostVisitedModuleParam, "true"},
        {kReducedSpaceParam, "-80"},
        {kHideIrrelevantModulesParam, "true"}};
const FeatureEntry::FeatureParam kMagicStackReducedNTPTopSpace[] = {
    {kMagicStackMostVisitedModuleParam, "false"},
    {kReducedSpaceParam, "20"}};
const FeatureEntry::FeatureParam
    kMagicStackReducedNTPTopSpaceHidIrrelevantModules[] = {
        {kMagicStackMostVisitedModuleParam, "false"},
        {kReducedSpaceParam, "20"},
        {kHideIrrelevantModulesParam, "true"}};

const FeatureEntry::FeatureVariation kMagicStackVariations[]{
    {"Most Visited Tiles in Magic Stack", kMagicStackMostVisitedModule,
     std::size(kMagicStackMostVisitedModule), nullptr},
    {"Most Visited Tiles in Magic Stack and hide irrelevant modules",
     kMagicStackMostVisitedModuleHideIrrelevantModules,
     std::size(kMagicStackMostVisitedModuleHideIrrelevantModules), nullptr},
    {"Magic Stack with less NTP Top Space", kMagicStackReducedNTPTopSpace,
     std::size(kMagicStackReducedNTPTopSpace), nullptr},
    {"Magic Stack with less NTP Top Space and hide irrelevant modules",
     kMagicStackReducedNTPTopSpaceHidIrrelevantModules,
     std::size(kMagicStackReducedNTPTopSpaceHidIrrelevantModules), nullptr},
};

const FeatureEntry::FeatureParam kEnableDefaultModel[] = {
    {segmentation_platform::kDefaultModelEnabledParam, "true"}};

const FeatureEntry::FeatureVariation
    kSegmentationPlatformIosModuleRankerVariations[]{
        {"Enabled With Default Model Parameter (Must Set this!)",
         kEnableDefaultModel, std::size(kEnableDefaultModel), nullptr},
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

const FeatureEntry::FeatureParam kTabPickupThresholdTenMinutes[] = {
    {kTabPickupThresholdParameterName, kTabPickupThresholdTenMinutesParam}};
const FeatureEntry::FeatureParam kTabPickupThresholdOneHour[] = {
    {kTabPickupThresholdParameterName, kTabPickupThresholdOneHourParam}};
const FeatureEntry::FeatureParam kTabPickupThresholdTwoHours[] = {
    {kTabPickupThresholdParameterName, kTabPickupThresholdTwoHoursParam}};
const FeatureEntry::FeatureParam kTabPickupNoFavicon[] = {
    {kTabPickupThresholdParameterName, kTabPickupNoFaviconParam}};

const FeatureEntry::FeatureVariation kTabPickupThresholdVariations[] = {
    {"Ten Minutes", kTabPickupThresholdTenMinutes,
     std::size(kTabPickupThresholdTenMinutes), nullptr},
    {"One Hour", kTabPickupThresholdOneHour,
     std::size(kTabPickupThresholdOneHour), nullptr},
    {"Two Hours", kTabPickupThresholdTwoHours,
     std::size(kTabPickupThresholdTwoHours), nullptr},
    {"No favicon", kTabPickupNoFavicon, std::size(kTabPickupNoFavicon),
     nullptr},
};

const FeatureEntry::FeatureParam kTabResumptionMostRecentTabOnly[] = {
    {kTabResumptionParameterName, kTabResumptionMostRecentTabOnlyParam}};
const FeatureEntry::FeatureParam kTabResumptionAllTabs[] = {
    {kTabResumptionParameterName, kTabResumptionAllTabsParam}};
const FeatureEntry::FeatureParam kTabResumptionAllTabsOneDayThreshold[] = {
    {kTabResumptionParameterName, kTabResumptionAllTabsOneDayThresholdParam}};

const FeatureEntry::FeatureVariation kTabResumptionVariations[] = {
    {"Most recent tab only", kTabResumptionMostRecentTabOnly,
     std::size(kTabResumptionMostRecentTabOnly), nullptr},
    {"Most recent tab and last synced tab (12 hours threshold)",
     kTabResumptionAllTabs, std::size(kTabResumptionAllTabs), nullptr},
    {"Most recent tab and last synced tab (24 hours threshold)",
     kTabResumptionAllTabsOneDayThreshold,
     std::size(kTabResumptionAllTabsOneDayThreshold), nullptr},
};

const FeatureEntry::FeatureParam
    kCredentialProviderExtensionPromoOnPasswordSaved[] = {
        {kCredentialProviderExtensionPromoOnPasswordSavedParam, "true"}};
const FeatureEntry::FeatureParam
    kCredentialProviderExtensionPromoOnPasswordCopied[] = {
        {kCredentialProviderExtensionPromoOnPasswordCopiedParam, "true"}};
const FeatureEntry::FeatureParam
    kCredentialProviderExtensionPromoOnAllTriggers[] = {
        {kCredentialProviderExtensionPromoOnPasswordCopiedParam, "true"},
        {kCredentialProviderExtensionPromoOnPasswordSavedParam, "true"}};

const FeatureEntry::FeatureVariation
    kCredentialProviderExtensionPromoVariations[] = {
        {"On password saved", kCredentialProviderExtensionPromoOnPasswordSaved,
         std::size(kCredentialProviderExtensionPromoOnPasswordSaved), nullptr},
        {"On password copied",
         kCredentialProviderExtensionPromoOnPasswordCopied,
         std::size(kCredentialProviderExtensionPromoOnPasswordCopied), nullptr},
        {"On all triggers", kCredentialProviderExtensionPromoOnAllTriggers,
         std::size(kCredentialProviderExtensionPromoOnAllTriggers), nullptr}};

const FeatureEntry::FeatureParam kIOSEditMenuPartialTranslateNoIncognito[] = {
    {kIOSEditMenuPartialTranslateNoIncognitoParam, "true"}};
const FeatureEntry::FeatureParam kIOSEditMenuPartialTranslateWithIncognito[] = {
    {kIOSEditMenuPartialTranslateNoIncognitoParam, "false"}};
const FeatureEntry::FeatureVariation kIOSEditMenuPartialTranslateVariations[] =
    {{"Disable on incognito", kIOSEditMenuPartialTranslateNoIncognito,
      std::size(kIOSEditMenuPartialTranslateNoIncognito), nullptr},
     {"Enable on incognito", kIOSEditMenuPartialTranslateWithIncognito,
      std::size(kIOSEditMenuPartialTranslateWithIncognito), nullptr}};

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

const FeatureEntry::FeatureParam kBottomOmniboxDefaultSettingTop[] = {
    {kBottomOmniboxDefaultSettingParam, kBottomOmniboxDefaultSettingParamTop}};
const FeatureEntry::FeatureParam kBottomOmniboxDefaultSettingBottom[] = {
    {kBottomOmniboxDefaultSettingParam,
     kBottomOmniboxDefaultSettingParamBottom}};
const FeatureEntry::FeatureParam kBottomOmniboxDefaultSettingSafariSwitcher[] =
    {{kBottomOmniboxDefaultSettingParam,
      kBottomOmniboxDefaultSettingParamSafariSwitcher}};
const FeatureEntry::FeatureVariation kBottomOmniboxDefaultSettingVariations[] =
    {
        {"Top", kBottomOmniboxDefaultSettingTop,
         std::size(kBottomOmniboxDefaultSettingTop), nullptr},
        {"Bottom", kBottomOmniboxDefaultSettingBottom,
         std::size(kBottomOmniboxDefaultSettingBottom), nullptr},
        {"Bottom for Safari Switcher",
         kBottomOmniboxDefaultSettingSafariSwitcher,
         std::size(kBottomOmniboxDefaultSettingSafariSwitcher), nullptr},
};

const FeatureEntry::FeatureParam kBottomOmniboxPromoForced[] = {
    {kBottomOmniboxPromoParam, kBottomOmniboxPromoParamForced}};
const FeatureEntry::FeatureVariation kBottomOmniboxPromoVariations[] = {
    {"Forced", kBottomOmniboxPromoForced, std::size(kBottomOmniboxPromoForced),
     nullptr},
};

const FeatureEntry::FeatureParam kBottomOmniboxPromoDefaultPositionTop[] = {
    {kBottomOmniboxPromoDefaultPositionParam,
     kBottomOmniboxPromoDefaultPositionParamTop}};
const FeatureEntry::FeatureParam kBottomOmniboxPromoDefaultPositionBottom[] = {
    {kBottomOmniboxPromoDefaultPositionParam,
     kBottomOmniboxPromoDefaultPositionParamBottom}};
const FeatureEntry::FeatureVariation
    kBottomOmniboxPromoDefaultPositionVariations[] = {
        {"Top", kBottomOmniboxPromoDefaultPositionTop,
         std::size(kBottomOmniboxPromoDefaultPositionTop), nullptr},
        {"Bottom", kBottomOmniboxPromoDefaultPositionBottom,
         std::size(kBottomOmniboxPromoDefaultPositionBottom), nullptr},
};

const FeatureEntry::Choice kReplaceSyncPromosWithSignInPromosChoices[] = {
    {"Default", "", ""},
    {"Disabled", "disable-features",
     "ReplaceSyncPromosWithSignInPromos,"
     "ConsistencyNewAccountInterface,"
     "FeedBottomSyncStringRemoval,"
     "SyncEnableContactInfoDataTypeInTransportMode,"
     "SyncEnableContactInfoDataTypeForCustomPassphraseUsers,"
     "SyncEnableBatchUploadLocalData,"
     "SyncEnableWalletMetadataInTransportMode,"
     "SyncEnableWalletOfferInTransportMode,"
     "IOSPasswordSettingsBulkUploadLocalPasswords"},
    {"Enabled without fast-follows", "enable-features",
     "ReplaceSyncPromosWithSignInPromos,"
     "ConsistencyNewAccountInterface,"
     "FeedBottomSyncStringRemoval,"
     "SyncEnableContactInfoDataTypeInTransportMode,"
     "SyncEnableContactInfoDataTypeForCustomPassphraseUsers,"
     "SyncEnableBatchUploadLocalData,"
     "SyncEnableWalletMetadataInTransportMode,"
     "SyncEnableWalletOfferInTransportMode,"
     "IOSPasswordSettingsBulkUploadLocalPasswords"},
    {"Enabled with fast-follows", "enable-features",
     "ReplaceSyncPromosWithSignInPromos,"
     "ConsistencyNewAccountInterface,"
     "FeedBottomSyncStringRemoval,"
     "SyncEnableContactInfoDataTypeInTransportMode,"
     "SyncEnableContactInfoDataTypeForCustomPassphraseUsers,"
     "SyncEnableBatchUploadLocalData,"
     "SyncEnableWalletMetadataInTransportMode,"
     "SyncEnableWalletOfferInTransportMode,"
     "IOSPasswordSettingsBulkUploadLocalPasswords,"
     "HistoryOptInForRestoreShortyAndReSignin,"
     "EnableBatchUploadFromBookmarksManager,"
     "EnableReviewAccountSettingsPromo"},
};

const FeatureEntry::FeatureParam kOneTapForMapsConsentModeDefault[] = {
    {web::features::kOneTapForMapsConsentModeParamTitle,
     web::features::kOneTapForMapsConsentModeDefaultParam}};
const FeatureEntry::FeatureParam kOneTapForMapsConsentModeForced[] = {
    {web::features::kOneTapForMapsConsentModeParamTitle,
     web::features::kOneTapForMapsConsentModeForcedParam}};
const FeatureEntry::FeatureParam kOneTapForMapsConsentModeDisabled[] = {
    {web::features::kOneTapForMapsConsentModeParamTitle,
     web::features::kOneTapForMapsConsentModeDisabledParam}};
const FeatureEntry::FeatureParam kOneTapForMapsConsentModeIPH[] = {
    {web::features::kOneTapForMapsConsentModeParamTitle,
     web::features::kOneTapForMapsConsentModeIPHParam}};
const FeatureEntry::FeatureParam kOneTapForMapsConsentModeIPHForced[] = {
    {web::features::kOneTapForMapsConsentModeParamTitle,
     web::features::kOneTapForMapsConsentModeIPHForcedParam}};
const FeatureEntry::FeatureVariation kOneTapForMapsWithVariations[] = {
    {"Consent Default", kOneTapForMapsConsentModeDefault,
     std::size(kOneTapForMapsConsentModeDefault), nullptr},
    {"Consent Forced", kOneTapForMapsConsentModeForced,
     std::size(kOneTapForMapsConsentModeForced), nullptr},
    {"Consent IPH", kOneTapForMapsConsentModeIPH,
     std::size(kOneTapForMapsConsentModeIPH), nullptr},
    {"Consent IPH forced", kOneTapForMapsConsentModeIPHForced,
     std::size(kOneTapForMapsConsentModeIPHForced), nullptr},
    {"Consent Disabled", kOneTapForMapsConsentModeDisabled,
     std::size(kOneTapForMapsConsentModeDisabled), nullptr},
};

constexpr FeatureEntry::FeatureParam kOmniboxInspireMeWith25Total5Trends[] = {
    {OmniboxFieldTrial::kInspireMeAdditionalTrendingQueries.name, "5"},
    {OmniboxFieldTrial::kInspireMePsuggestQueries.name, "20"}};
constexpr FeatureEntry::FeatureParam kOmniboxInspireMeWith20Total5Trends[] = {
    {OmniboxFieldTrial::kInspireMeAdditionalTrendingQueries.name, "5"},
    {OmniboxFieldTrial::kInspireMePsuggestQueries.name, "15"}};
constexpr FeatureEntry::FeatureParam kOmniboxInspireMeWith25Total10Trends[] = {
    {OmniboxFieldTrial::kInspireMeAdditionalTrendingQueries.name, "10"},
    {OmniboxFieldTrial::kInspireMePsuggestQueries.name, "15"}};
constexpr FeatureEntry::FeatureParam kOmniboxInspireMeWith20Total10Trends[] = {
    {OmniboxFieldTrial::kInspireMeAdditionalTrendingQueries.name, "10"},
    {OmniboxFieldTrial::kInspireMePsuggestQueries.name, "10"}};

constexpr FeatureEntry::FeatureVariation kOmniboxInspireMeVariants[] = {
    {"25 total, 5 Trends", kOmniboxInspireMeWith25Total5Trends,
     std::size(kOmniboxInspireMeWith25Total5Trends), "t3363282"},
    {"20 total, 5 Trends", kOmniboxInspireMeWith20Total5Trends,
     std::size(kOmniboxInspireMeWith20Total5Trends), "t3363282"},
    {"25 total, 10 Trends", kOmniboxInspireMeWith25Total10Trends,
     std::size(kOmniboxInspireMeWith25Total10Trends), "t3363285"},
    {"20 total, 10 Trends", kOmniboxInspireMeWith20Total10Trends,
     std::size(kOmniboxInspireMeWith20Total10Trends), "t3363285"},
};

const FeatureEntry::Choice kEnablePasswordSharingChoices[] = {
    {"Default", "", ""},
    {"Bootstraping Only", switches::kEnableFeatures,
     "SharingOfferKeyPairBootstrap"},
    {"Enabled", switches::kEnableFeatures,
     "SharingOfferKeyPairBootstrap,SendPasswords,"
     "PasswordManagerEnableSenderService,"
     "PasswordManagerEnableReceiverService,SharedPasswordNotificationUI"},
};

const FeatureEntry::FeatureParam kIOSHideFeedWithSearchChoiceTargetedParams[] =
    {{kIOSHideFeedWithSearchChoiceTargeted, "true"}};
const FeatureEntry::FeatureVariation kIOSHideFeedWithSearchChoiceVariations[]{
    {"with targeting", kIOSHideFeedWithSearchChoiceTargetedParams,
     std::size(kIOSHideFeedWithSearchChoiceTargetedParams), nullptr},
};

const flags_ui::FeatureEntry::FeatureParam kParcelTrackingTestDataDelivered[] =
    {{commerce::kParcelTrackingTestDataParam,
      commerce::kParcelTrackingTestDataParamDelivered}};
const flags_ui::FeatureEntry::FeatureParam kParcelTrackingTestDataInProgress[] =
    {{commerce::kParcelTrackingTestDataParam,
      commerce::kParcelTrackingTestDataParamInProgress}};
const flags_ui::FeatureEntry::FeatureParam
    kParcelTrackingTestDataOutForDelivery[] = {
        {commerce::kParcelTrackingTestDataParam,
         commerce::kParcelTrackingTestDataParamOutForDelivery}};
const flags_ui::FeatureEntry::FeatureVariation
    kParcelTrackingTestDataVariations[] = {
        {"Delivered", kParcelTrackingTestDataDelivered,
         std::size(kParcelTrackingTestDataDelivered), nullptr},
        {"In progress", kParcelTrackingTestDataInProgress,
         std::size(kParcelTrackingTestDataInProgress), nullptr},
        {"Out for delivery", kParcelTrackingTestDataOutForDelivery,
         std::size(kParcelTrackingTestDataOutForDelivery), nullptr},
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
    {"omnibox-inspire-me", flag_descriptions::kOmniboxInspireMeName,
     flag_descriptions::kOmniboxInspireMeDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kInspireMe,
                                    kOmniboxInspireMeVariants,
                                    "OmniboxBundledExperimentV1")},
    {"omnibox-inspire-me-signed-out",
     flag_descriptions::kOmniboxInspireMeSignedOutName,
     flag_descriptions::kOmniboxInspireMeSignedOutDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kZeroSuggestOnNTPForSignedOutUsers)},
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
    {"omnibox-report-assisted-query-stats",
     flag_descriptions::kOmniboxReportAssistedQueryStatsName,
     flag_descriptions::kOmniboxReportAssistedQueryStatsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kReportAssistedQueryStats)},
    {"omnibox-report-searchbox-stats",
     flag_descriptions::kOmniboxReportSearchboxStatsName,
     flag_descriptions::kOmniboxReportSearchboxStatsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kReportSearchboxStats)},
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
    {"incognito-ntp-revamp", flag_descriptions::kIncognitoNtpRevampName,
     flag_descriptions::kIncognitoNtpRevampDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIncognitoNtpRevamp)},
    {"wait-threshold-seconds-for-capabilities-api",
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiName,
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kWaitThresholdMillisecondsForCapabilitiesApiChoices)},
    {"new-overflow-menu", flag_descriptions::kNewOverflowMenuName,
     flag_descriptions::kNewOverflowMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewOverflowMenu)},
    {"content-push-notifications",
     flag_descriptions::kContentPushNotificationsName,
     flag_descriptions::kContentPushNotificationsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kContentPushNotifications,
                                    kContentPushNotificationsVariations,
                                    "ContentPushNotifications")},
    {"overflow-menu-customization",
     flag_descriptions::kOverflowMenuCustomizationName,
     flag_descriptions::kOverflowMenuCustomizationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOverflowMenuCustomization)},
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
    {"autofill-enable-card-product-name",
     flag_descriptions::kAutofillEnableCardProductNameName,
     flag_descriptions::kAutofillEnableCardProductNameDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardProductName)},
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
    {"intents-on-measurements", flag_descriptions::kMeasurementsName,
     flag_descriptions::kMeasurementsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableMeasurements)},
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
     FEATURE_WITH_PARAMS_VALUE_TYPE(web::features::kOneTapForMaps,
                                    kOneTapForMapsWithVariations,
                                    "OneTapForMaps")},
    {"enable-annotations-language-detection",
     flag_descriptions::kUseAnnotationsForLanguageDetectionName,
     flag_descriptions::kUseAnnotationsForLanguageDetectionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kUseAnnotationsForLanguageDetection)},
    {"omnibox-https-upgrades", flag_descriptions::kOmniboxHttpsUpgradesName,
     flag_descriptions::kOmniboxHttpsUpgradesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kDefaultTypedNavigationsToHttps)},
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
    {"content-suggestions-magic-stack", flag_descriptions::kMagicStackName,
     flag_descriptions::kMagicStackDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kMagicStack,
                                    kMagicStackVariations,
                                    flag_descriptions::kMagicStackName)},
    {"ios-hide-feed-with-search-choice",
     flag_descriptions::kIOSHideFeedWithSearchChoiceName,
     flag_descriptions::kIOSHideFeedWithSearchChoiceDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kIOSHideFeedWithSearchChoice,
         kIOSHideFeedWithSearchChoiceVariations,
         flag_descriptions::kIOSHideFeedWithSearchChoiceName)},
    {"ios-keyboard-accessory-upgrade",
     flag_descriptions::kIOSKeyboardAccessoryUpgradeName,
     flag_descriptions::kIOSKeyboardAccessoryUpgradeDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSKeyboardAccessoryUpgrade)},
    {"ios-large-fakebox", flag_descriptions::kIOSLargeFakeboxName,
     flag_descriptions::kIOSLargeFakeboxDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSLargeFakebox)},
    {"ios-magic-stack-segmentation-ranking",
     flag_descriptions::kSegmentationPlatformIosModuleRankerName,
     flag_descriptions::kSegmentationPlatformIosModuleRankerDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         segmentation_platform::features::kSegmentationPlatformIosModuleRanker,
         kSegmentationPlatformIosModuleRankerVariations,
         flag_descriptions::kSegmentationPlatformIosModuleRankerName)},
    {"default-browser-intents-show-settings",
     flag_descriptions::kDefaultBrowserIntentsShowSettingsName,
     flag_descriptions::kDefaultBrowserIntentsShowSettingsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDefaultBrowserIntentsShowSettings)},
    {"ios-password-bottom-sheet",
     flag_descriptions::kIOSPasswordBottomSheetName,
     flag_descriptions::kIOSPasswordBottomSheetDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSPasswordBottomSheet)},
    {"ios-password-bottom-sheet-autofocus",
     flag_descriptions::kIOSPasswordBottomSheetAutofocusName,
     flag_descriptions::kIOSPasswordBottomSheetAutofocusDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kIOSPasswordBottomSheetAutofocus)},
    {"ios-payments-bottom-sheet",
     flag_descriptions::kIOSPaymentsBottomSheetName,
     flag_descriptions::kIOSPaymentsBottomSheetDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSPaymentsBottomSheet)},
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
    {"enable-button-configuration-usage",
     flag_descriptions::kEnableUIButtonConfigurationName,
     flag_descriptions::kEnableUIButtonConfigurationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableUIButtonConfiguration)},
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
     FEATURE_VALUE_TYPE(kDefaultBrowserTriggerCriteriaExperiment)},
    {"default-browser-video-in-settings",
     flag_descriptions::kDefaultBrowserVideoInSettingsName,
     flag_descriptions::kDefaultBrowserVideoInSettingsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDefaultBrowserVideoInSettings)},
    {"fullscreen-promo-on-omnibox-copy-paste",
     flag_descriptions::kFullScreenPromoOnOmniboxCopyPasteName,
     flag_descriptions::kFullScreenPromoOnOmniboxCopyPasteDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kFullScreenPromoOnOmniboxCopyPaste)},
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
    {"app-store-rating", flag_descriptions::kAppStoreRatingName,
     flag_descriptions::kAppStoreRatingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAppStoreRating)},
    {"enable-tflite-language-detection-ignore",
     flag_descriptions::kTFLiteLanguageDetectionIgnoreName,
     flag_descriptions::kTFLiteLanguageDetectionIgnoreDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(translate::kTFLiteLanguageDetectionIgnoreEnabled)},
    {"tab-grid-new-transitions", flag_descriptions::kTabGridNewTransitionsName,
     flag_descriptions::kTabGridNewTransitionsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridNewTransitions)},
    {"credential-provider-extension-promo",
     flag_descriptions::kCredentialProviderExtensionPromoName,
     flag_descriptions::kCredentialProviderExtensionPromoDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kCredentialProviderExtensionPromo,
                                    kCredentialProviderExtensionPromoVariations,
                                    "CredentialProviderExtensionPromo")},
    {"iph-price-notifications-while-browsing",
     flag_descriptions::kIPHPriceNotificationsWhileBrowsingName,
     flag_descriptions::kIPHPriceNotificationsWhileBrowsingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         feature_engagement::kIPHPriceNotificationsWhileBrowsingFeature)},
    {"autofill-suggest-server-card-instead-of-local-card",
     flag_descriptions::kAutofillSuggestServerCardInsteadOfLocalCardName,
     flag_descriptions::kAutofillSuggestServerCardInsteadOfLocalCardDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSuggestServerCardInsteadOfLocalCard)},
    {"promos-manager-uses-fet", flag_descriptions::kPromosManagerUsesFETName,
     flag_descriptions::kPromosManagerUsesFETDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPromosManagerUsesFET)},
    {"shopping-collection",
     commerce::flag_descriptions::kShoppingCollectionName,
     commerce::flag_descriptions::kShoppingCollectionDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(commerce::kShoppingCollection)},
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
    {"enable-friendlier-safe-browsing-settings-enhanced-protection",
     flag_descriptions::
         kEnableFriendlierSafeBrowsingSettingsEnhancedProtectionName,
     flag_descriptions::
         kEnableFriendlierSafeBrowsingSettingsEnhancedProtectionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         safe_browsing::kFriendlierSafeBrowsingSettingsEnhancedProtection)},
    {"enable-friendlier-safe-browsing-settings-standard-protection",
     flag_descriptions::
         kEnableFriendlierSafeBrowsingSettingsStandardProtectionName,
     flag_descriptions::
         kEnableFriendlierSafeBrowsingSettingsStandardProtectionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         safe_browsing::kFriendlierSafeBrowsingSettingsStandardProtection)},
    {"enable-red-interstitial-facelift",
     flag_descriptions::kEnableRedInterstitialFaceliftName,
     flag_descriptions::kEnableRedInterstitialFaceliftDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(safe_browsing::kRedInterstitialFacelift)},
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
    {"notification-settings-menu-item",
     flag_descriptions::kNotificationSettingsMenuItemName,
     flag_descriptions::kNotificationSettingsMenuItemDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNotificationSettingsMenuItem)},
    {"feed-experiment-tagging-ios",
     flag_descriptions::kFeedExperimentTaggingName,
     flag_descriptions::kFeedExperimentTaggingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFeedExperimentTagging)},
    {"spotlight-reading-list-source",
     flag_descriptions::kSpotlightReadingListSourceName,
     flag_descriptions::kSpotlightReadingListSourceDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kSpotlightReadingListSource)},
    {"enable-policy-test-page-ios",
     flag_descriptions::kEnablePolicyTestPageName,
     flag_descriptions::kEnablePolicyTestPageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(policy::features::kEnablePolicyTestPage)},
    {"enable-bookmarks-account-storage",
     flag_descriptions::kEnableBookmarksAccountStorageName,
     flag_descriptions::kEnableBookmarksAccountStorageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kEnableBookmarksAccountStorage)},
    {"web-feed-feedback-reroute",
     flag_descriptions::kWebFeedFeedbackRerouteName,
     flag_descriptions::kWebFeedFeedbackRerouteDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kWebFeedFeedbackReroute)},
    {"new-ntp-omnibox-layout", flag_descriptions::kNewNTPOmniboxLayoutName,
     flag_descriptions::kNewNTPOmniboxLayoutDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewNTPOmniboxLayout)},
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
    {"enable-preferences-account-storage",
     flag_descriptions::kEnablePreferencesAccountStorageName,
     flag_descriptions::kEnablePreferencesAccountStorageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kEnablePreferencesAccountStorage)},
    {"ios-browser-edit-menu-metrics",
     flag_descriptions::kIOSBrowserEditMenuMetricsName,
     flag_descriptions::kIOSBrowserEditMenuMetricsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSBrowserEditMenuMetrics)},
    {"ios-bulk-upload-local-passwords",
     flag_descriptions::kIOSPasswordSettingsBulkUploadLocalPasswordsName,
     flag_descriptions::kIOSPasswordSettingsBulkUploadLocalPasswordsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kIOSPasswordSettingsBulkUploadLocalPasswords)},
    {"enable-reading-list-sign-in-promo",
     flag_descriptions::kEnableReadingListSignInPromoName,
     flag_descriptions::kEnableReadingListSignInPromoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kReadingListEnableSyncTransportModeUponSignIn)},
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
    {"only-access-clipboard-async",
     flag_descriptions::kOnlyAccessClipboardAsyncName,
     flag_descriptions::kOnlyAccessClipboardAsyncDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOnlyAccessClipboardAsync)},
    {"omnibox-tail-suggest", flag_descriptions::kOmniboxTailSuggestName,
     flag_descriptions::kOmniboxTailSuggestDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOmniboxTailSuggest)},
    {"omnibox-improved-rtl-suggestions",
     flag_descriptions::kOmniboxSuggestionsRTLImprovementsName,
     flag_descriptions::kOmniboxSuggestionsRTLImprovementsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kOmniboxSuggestionsRTLImprovements)},
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
    {"spotlight-donate-new-intents",
     flag_descriptions::kSpotlightDonateNewIntentsName,
     flag_descriptions::kSpotlightDonateNewIntentsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSpotlightDonateNewIntents)},
    {"replace-sync-promos-with-sign-in-promos",
     flag_descriptions::kReplaceSyncPromosWithSignInPromosName,
     flag_descriptions::kReplaceSyncPromosWithSignInPromosDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kReplaceSyncPromosWithSignInPromosChoices)},
    {"tab-grid-refactoring", flag_descriptions::kTabGridRefactoringName,
     flag_descriptions::kTabGridRefactoringDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridRefactoring)},
    {"tab-pickup-threshold", flag_descriptions::kTabPickupThresholdName,
     flag_descriptions::kTabPickupThresholdDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kTabPickupThreshold,
                                    kTabPickupThresholdVariations,
                                    "TabPickupThreshold")},
    {"ios-iph-for-safari-switcher",
     flag_descriptions::kIPHForSafariSwitcherName,
     flag_descriptions::kIPHForSafariSwitcherDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIPHForSafariSwitcher)},
    {"safety-check-magic-stack", flag_descriptions::kSafetyCheckMagicStackName,
     flag_descriptions::kSafetyCheckMagicStackDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSafetyCheckMagicStack)},
    {"app-store-rating-loosened-triggers",
     flag_descriptions::kAppStoreRatingLoosenedTriggersName,
     flag_descriptions::kAppStoreRatingLoosenedTriggersDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAppStoreRatingLoosenedTriggers)},
    {"ios-password-auth-on-entry",
     flag_descriptions::kIOSPasswordAuthOnEntryName,
     flag_descriptions::kIOSPasswordAuthOnEntryDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSPasswordAuthOnEntry)},
    {"tab-resumption", flag_descriptions::kTabResumptionName,
     flag_descriptions::kTabResumptionDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kTabResumption,
                                    kTabResumptionVariations,
                                    "TabResumption")},
    {"bottom-omnibox-default-setting",
     flag_descriptions::kBottomOmniboxDefaultSettingName,
     flag_descriptions::kBottomOmniboxDefaultSettingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBottomOmniboxDefaultSetting,
                                    kBottomOmniboxDefaultSettingVariations,
                                    "BottomOmniboxDefaultSetting")},
    {"tab-pickup-minimum-delay", flag_descriptions::kTabPickupMinimumDelayName,
     flag_descriptions::kTabPickupMinimumDelayDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabPickupMinimumDelay)},
    {"enable-family-link-controls",
     flag_descriptions::kEnableFamilyLinkControlsName,
     flag_descriptions::kEnableFamilyLinkControlsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS)},
    {"discover-feed-sport-card", flag_descriptions::kDiscoverFeedSportCardName,
     flag_descriptions::kDiscoverFeedSportCardDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDiscoverFeedSportCard)},
    {"ios-password-auth-on-entry-v2",
     flag_descriptions::kIOSPasswordAuthOnEntryV2Name,
     flag_descriptions::kIOSPasswordAuthOnEntryV2Description, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSPasswordAuthOnEntryV2)},
    {"enable-save-to-photos", flag_descriptions::kIOSSaveToPhotosName,
     flag_descriptions::kIOSSaveToPhotosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSSaveToPhotos)},
    {"ios-parcel-tracking", flag_descriptions::kIOSParcelTrackingName,
     flag_descriptions::kIOSParcelTrackingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSParcelTracking)},
    {"parcel-tracking-test-data",
     commerce::flag_descriptions::kParcelTrackingTestDataName,
     commerce::flag_descriptions::kParcelTrackingTestDataDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kParcelTrackingTestData,
                                    kParcelTrackingTestDataVariations,
                                    "ParcelTrackingTestData")},
    {"autofill-enable-merchant-domain-in-unmask-card-request",
     flag_descriptions::kAutofillEnableMerchantDomainInUnmaskCardRequestName,
     flag_descriptions::
         kAutofillEnableMerchantDomainInUnmaskCardRequestDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableMerchantDomainInUnmaskCardRequest)},
    {"autofill-update-chrome-settings-link-to-gpay-web",
     flag_descriptions::kAutofillUpdateChromeSettingsLinkToGPayWebName,
     flag_descriptions::kAutofillUpdateChromeSettingsLinkToGPayWebDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpdateChromeSettingsLinkToGPayWeb)},
    {"top-toolbar-theme-color", flag_descriptions::kThemeColorInTopToolbarName,
     flag_descriptions::kThemeColorInTopToolbarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kThemeColorInTopToolbar)},
    {"iph-ios-promo-manager-widget-promo",
     flag_descriptions::kIPHiOSPromoPasswordManagerWidgetName,
     flag_descriptions::kIPHiOSPromoPasswordManagerWidgetDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         feature_engagement::kIPHiOSPromoPasswordManagerWidgetFeature)},
    {"autofill-enable-virtual-cards",
     flag_descriptions::kAutofillEnableVirtualCardsName,
     flag_descriptions::kAutofillEnableVirtualCardsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableVirtualCards)},
    {"ios-incognito-downloads-warning",
     flag_descriptions::kIOSIncognitoDownloadsWarningName,
     flag_descriptions::kIOSIncognitoDownloadsWarningDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSIncognitoDownloadsWarning)},
    {"omnibox-shortcuts-database-ios",
     flag_descriptions::kOmniboxPopulateShortcutsDatabaseName,
     flag_descriptions::kOmniboxPopulateShortcutsDatabaseDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxPopulateShortcutsDatabase)},
    {"enable-feed-containment", flag_descriptions::kEnableFeedContainmentName,
     flag_descriptions::kEnableFeedContainmentDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFeedContainment)},
    {"delete-undecryptable-passwords",
     flag_descriptions::kClearUndecryptablePasswordsOnSyncName,
     flag_descriptions::kClearUndecryptablePasswordsOnSyncDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kClearUndecryptablePasswordsOnSync)},
    {"ignore-undecryptable-passwords",
     flag_descriptions::kSkipUndecryptablePasswordsName,
     flag_descriptions::kSkipUndecryptablePasswordsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kSkipUndecryptablePasswords)},
    {"privacy-guide-ios", flag_descriptions::kPrivacyGuideIosName,
     flag_descriptions::kPrivacyGuideIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPrivacyGuideIos)},
    {"bottom-omnibox-device-switcher-results",
     flag_descriptions::kBottomOmniboxDeviceSwitcherResultsName,
     flag_descriptions::kBottomOmniboxDeviceSwitcherResultsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kBottomOmniboxDeviceSwitcherResults)},
    {"sync-session-on-visibility-changed",
     flag_descriptions::kSyncSessionOnVisibilityChangedName,
     flag_descriptions::kSyncSessionOnVisibilityChangedDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncSessionOnVisibilityChanged)},
    {"enable-save-to-drive", flag_descriptions::kIOSSaveToDriveName,
     flag_descriptions::kIOSSaveToDriveDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSSaveToDrive)},
    {"password-sharing", flag_descriptions::kPasswordSharingName,
     flag_descriptions::kPasswordSharingDescription, flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kEnablePasswordSharingChoices)},
    {"omnibox-company-entity-icon-adjustment",
     flag_descriptions::kOmniboxCompanyEntityIconAdjustmentName,
     flag_descriptions::kOmniboxCompanyEntityIconAdjustmentDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kCompanyEntityIconAdjustment,
                                    kOmniboxCompanyEntityAdjustmentVariations,
                                    "OmniboxCompanyEntityAdjustment")},
    {"dynamic-theme-color", flag_descriptions::kDynamicThemeColorName,
     flag_descriptions::kDynamicThemeColorDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDynamicThemeColor)},
    {"dynamic-background-color", flag_descriptions::kDynamicBackgroundColorName,
     flag_descriptions::kDynamicBackgroundColorDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDynamicBackgroundColor)},
    {"fullscreen-improvement", flag_descriptions::kFullscreenImprovementName,
     flag_descriptions::kFullscreenImprovementDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenImprovement)},
    {"tab-groups-in-grid", flag_descriptions::kTabGroupsInGridName,
     flag_descriptions::kTabGroupsInGridDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGroupsInGrid)},
    {"autofill-enable-payments-mandatory-reauth",
     flag_descriptions::kAutofillEnablePaymentsMandatoryReauthName,
     flag_descriptions::kAutofillEnablePaymentsMandatoryReauthDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnablePaymentsMandatoryReauth)},
    {"autofill-enable-card-benefits",
     flag_descriptions::kAutofillEnableCardBenefitsName,
     flag_descriptions::kAutofillEnableCardBenefitsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardBenefits)},
    {"password-manager-signin-uff",
     flag_descriptions::kIOSPasswordSignInUffName,
     flag_descriptions::kIOSPasswordSignInUffDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSPasswordSignInUff)},
    {"tab-grid-compositional-layout",
     flag_descriptions::kTabGridCompositionalLayoutName,
     flag_descriptions::kTabGridCompositionalLayoutDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kTabGridCompositionalLayout)},
    {"bottom-omnibox-promo-fre", flag_descriptions::kBottomOmniboxPromoFREName,
     flag_descriptions::kBottomOmniboxPromoFREDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBottomOmniboxPromoFRE,
                                    kBottomOmniboxPromoVariations,
                                    "BottomOmniboxPromoFRE")},
    {"bottom-omnibox-promo-app-launch",
     flag_descriptions::kBottomOmniboxPromoAppLaunchName,
     flag_descriptions::kBottomOmniboxPromoAppLaunchDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBottomOmniboxPromoAppLaunch,
                                    kBottomOmniboxPromoVariations,
                                    "BottomOmniboxPromoAppLaunch")},
    {"bottom-omnibox-promo-default-position",
     flag_descriptions::kBottomOmniboxPromoDefaultPositionName,
     flag_descriptions::kBottomOmniboxPromoDefaultPositionDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kBottomOmniboxPromoDefaultPosition,
         kBottomOmniboxPromoDefaultPositionVariations,
         "BottomOmniboxPromoDefaultPosition")},
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

  if ([defaults boolForKey:@"EnableUserPolicyMerge"]) {
    NSString* user_policy_merge_key =
        base::SysUTF8ToNSString(policy::key::kCloudUserPolicyMerge);
    [testing_policies addEntriesFromDictionary:@{user_policy_merge_key : @YES}];
    [allowed_experimental_policies addObject:user_policy_merge_key];
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
