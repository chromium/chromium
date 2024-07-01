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
#import "components/autofill/ios/common/features.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/browser_sync/browser_sync_switches.h"
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
#import "components/omnibox/browser/omnibox_feature_configs.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/page_content_annotations/core/page_content_annotations_features.h"
#import "components/page_image_service/features.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/payments/core/features.h"
#import "components/policy/core/common/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
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
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/follow/model/follow_features.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/features.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/price_insights/model/price_insights_feature.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/screen_time/model/screen_time_buildflags.h"
#import "ios/chrome/browser/sessions/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/features.h"
#import "ios/chrome/browser/text_selection/model/text_selection_util.h"
#import "ios/chrome/browser/ui/download/features.h"
#import "ios/chrome/browser/ui/lens/features.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/ui/settings/google_services/features.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/web/model/features.h"
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

const FeatureEntry::Choice kRevampPageInfoiOSChoices[] = {
    {"Default", "", ""},
    {"Enabled", switches::kEnableFeatures,
     "RevampPageInfoIos, PageInfoAboutThisSite"},
    {"Disabled", switches::kDisableFeatures,
     "RevampPageInfoIos, PageInfoAboutThisSite"},
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
const FeatureEntry::FeatureParam kContentPushNotificationsEnabledProvisional[] =
    {{kContentPushNotificationsExperimentType, "3"}};
const FeatureEntry::FeatureParam
    kContentPushNotificationsPromoRegistrationOnly[] = {
        {kContentPushNotificationsExperimentType, "5"}};
const FeatureEntry::FeatureParam
    kContentPushNotificationsProvisionalRegistrationOnly[] = {
        {kContentPushNotificationsExperimentType, "6"}};
const FeatureEntry::FeatureParam
    kContentPushNotificationsSetUpListRegistrationOnly[] = {
        {kContentPushNotificationsExperimentType, "7"}};

const FeatureEntry::FeatureVariation kContentPushNotificationsVariations[] = {
    {"Promo", kContentPushNotificationsEnabledPromo,
     std::size(kContentPushNotificationsEnabledPromo), nullptr},
    {"Set up list", kContentPushNotificationsEnabledSetupLists,
     std::size(kContentPushNotificationsEnabledSetupLists), nullptr},
    {"Provisional Notification", kContentPushNotificationsEnabledProvisional,
     std::size(kContentPushNotificationsEnabledProvisional), nullptr},
    {"Promo Registeration Only", kContentPushNotificationsPromoRegistrationOnly,
     std::size(kContentPushNotificationsPromoRegistrationOnly), nullptr},
    {"Provisional Notification Registeration Only",
     kContentPushNotificationsProvisionalRegistrationOnly,
     std::size(kContentPushNotificationsProvisionalRegistrationOnly), nullptr},
    {"Set up list Registeration Only",
     kContentPushNotificationsSetUpListRegistrationOnly,
     std::size(kContentPushNotificationsSetUpListRegistrationOnly), nullptr}};

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
const FeatureEntry::FeatureParam kMagicStackHidIrrelevantModules[] = {
    {kMagicStackMostVisitedModuleParam, "false"},
    {kHideIrrelevantModulesParam, "true"}};

const FeatureEntry::FeatureVariation kMagicStackVariations[]{
    {"Most Visited Tiles in Magic Stack", kMagicStackMostVisitedModule,
     std::size(kMagicStackMostVisitedModule), nullptr},
    {"Most Visited Tiles in Magic Stack and hide irrelevant modules",
     kMagicStackMostVisitedModuleHideIrrelevantModules,
     std::size(kMagicStackMostVisitedModuleHideIrrelevantModules), nullptr},
    {"Hide irrelevant modules", kMagicStackHidIrrelevantModules,
     std::size(kMagicStackHidIrrelevantModules), nullptr},
};

const FeatureEntry::FeatureParam kEnableDefaultModel[] = {
    {segmentation_platform::kDefaultModelEnabledParam, "true"}};

const FeatureEntry::FeatureVariation
    kSegmentationPlatformIosModuleRankerVariations[]{
        {"Enabled With Default Model Parameter (Must Set this!)",
         kEnableDefaultModel, std::size(kEnableDefaultModel), nullptr},
    };

const FeatureEntry::FeatureParam kIOSTipsNotifications5SecondTrigger[] = {
    {kIOSTipsNotificationsTriggerTimeParam, "5s"},
};
const FeatureEntry::FeatureParam kIOSTipsNotifications10SecondTrigger[] = {
    {kIOSTipsNotificationsTriggerTimeParam, "10s"},
};
const FeatureEntry::FeatureParam kIOSTipsNotifications30SecondTrigger[] = {
    {kIOSTipsNotificationsTriggerTimeParam, "30s"},
};
const FeatureEntry::FeatureVariation kIOSTipsNotificationsVariations[] = {
    {"(5s trigger)", kIOSTipsNotifications5SecondTrigger,
     std::size(kIOSTipsNotifications10SecondTrigger), nullptr},
    {"(10s trigger)", kIOSTipsNotifications10SecondTrigger,
     std::size(kIOSTipsNotifications10SecondTrigger), nullptr},
    {"(30s trigger)", kIOSTipsNotifications30SecondTrigger,
     std::size(kIOSTipsNotifications10SecondTrigger), nullptr},
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

const FeatureEntry::FeatureParam kEnableExpKitTextClassifierAddressOneTap[] = {
    {kTextClassifierAddressParameterName, "true"}};
const FeatureEntry::FeatureVariation
    kEnableExpKitTextClassifierAddressVariations[] = {
        {"Long-Press and One-Tap", kEnableExpKitTextClassifierAddressOneTap,
         std::size(kEnableExpKitTextClassifierAddressOneTap), nullptr}};

const FeatureEntry::FeatureParam
    kEnableExpKitTextClassifierPhoneNumberOneTap[] = {
        {kTextClassifierPhoneNumberParameterName, "true"}};
const FeatureEntry::FeatureVariation
    kEnableExpKitTextClassifierPhoneNumberVariations[] = {
        {"Long-Press and One-Tap", kEnableExpKitTextClassifierPhoneNumberOneTap,
         std::size(kEnableExpKitTextClassifierPhoneNumberOneTap), nullptr}};

const FeatureEntry::FeatureParam kEnableExpKitTextClassifierEmailOneTap[] = {
    {kTextClassifierEmailParameterName, "true"}};
const FeatureEntry::FeatureVariation
    kEnableExpKitTextClassifierEmailVariations[] = {
        {"Long-Press and One-Tap", kEnableExpKitTextClassifierEmailOneTap,
         std::size(kEnableExpKitTextClassifierEmailOneTap), nullptr}};

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
     std::size(kTabResumptionAllTabsOneDayThreshold), nullptr}};

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

constexpr flags_ui::FeatureEntry::FeatureParam kPriceInsightsPriceIsLowParam[] =
    {{kLowPriceParam, kLowPriceParamPriceIsLow}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kPriceInsightsGoodDealNowParam[] = {
        {kLowPriceParam, kLowPriceParamGoodDealNow}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kPriceInsightsSeePriceHistoryParam[] = {
        {kLowPriceParam, kLowPriceParamSeePriceHistory}};
constexpr flags_ui::FeatureEntry::FeatureVariation kPriceInsightsVariations[] =
    {{"Price is low", kPriceInsightsPriceIsLowParam,
      std::size(kPriceInsightsPriceIsLowParam), nullptr},
     {"Good deal now", kPriceInsightsGoodDealNowParam,
      std::size(kPriceInsightsGoodDealNowParam), nullptr},
     {"See price history", kPriceInsightsSeePriceHistoryParam,
      std::size(kPriceInsightsSeePriceHistoryParam), nullptr}};

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

const FeatureEntry::FeatureParam kRichAutocompletionImplementationLabel[] = {
    {kRichAutocompletionParam, kRichAutocompletionParamLabel}};
const FeatureEntry::FeatureParam
    kRichAutocompletionImplementationTextField3Chars[] = {
        {kRichAutocompletionParam, kRichAutocompletionParamTextField},
        {"RichAutocompletionAutocompleteShortcutTextMinChar", "3"},
        {"RichAutocompletionAutocompleteTitlesMinChar", "3"}};
const FeatureEntry::FeatureParam
    kRichAutocompletionImplementationTextField4Chars[] = {
        {kRichAutocompletionParam, kRichAutocompletionParamTextField},
        {"RichAutocompletionAutocompleteShortcutTextMinChar", "4"},
        {"RichAutocompletionAutocompleteTitlesMinChar", "4"}};
const FeatureEntry::FeatureParam
    kRichAutocompletionImplementationNoAdditionalText3Chars[] = {
        {kRichAutocompletionParam, kRichAutocompletionParamNoAdditionalText},
        {"RichAutocompletionAutocompleteShortcutTextMinChar", "3"},
        {"RichAutocompletionAutocompleteTitlesMinChar", "3"}};
const FeatureEntry::FeatureParam
    kRichAutocompletionImplementationNoAdditionalText4Chars[] = {
        {kRichAutocompletionParam, kRichAutocompletionParamNoAdditionalText},
        {"RichAutocompletionAutocompleteShortcutTextMinChar", "4"},
        {"RichAutocompletionAutocompleteTitlesMinChar", "4"}};
const FeatureEntry::FeatureVariation
    kRichAutocompletionImplementationVariations[] = {
        {"In Label", kRichAutocompletionImplementationLabel,
         std::size(kRichAutocompletionImplementationLabel), nullptr},
        {"In TextField, 3 Min Chars",
         kRichAutocompletionImplementationTextField3Chars,
         std::size(kRichAutocompletionImplementationTextField3Chars), nullptr},
        {"In TextField, 4 Min Chars",
         kRichAutocompletionImplementationTextField4Chars,
         std::size(kRichAutocompletionImplementationTextField4Chars), nullptr},
        {"No Additional Text, 3 Min Chars",
         kRichAutocompletionImplementationNoAdditionalText3Chars,
         std::size(kRichAutocompletionImplementationNoAdditionalText3Chars),
         nullptr},
        {"No Additional Text, 4 Min Chars",
         kRichAutocompletionImplementationNoAdditionalText4Chars,
         std::size(kRichAutocompletionImplementationNoAdditionalText4Chars),
         nullptr},
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

const FeatureEntry::FeatureParam kIOSDockingPromoDisplayedAfterFRE[] = {
    {kIOSDockingPromoExperimentType, "0"}};
const FeatureEntry::FeatureParam kIOSDockingPromoDisplayedAtAppLaunch[] = {
    {kIOSDockingPromoExperimentType, "1"}};
const FeatureEntry::FeatureParam kIOSDockingPromoDisplayedDuringFRE[] = {
    {kIOSDockingPromoExperimentType, "2"}};

const FeatureEntry::FeatureVariation kIOSDockingPromoVariations[] = {
    {"Display promo after FRE", kIOSDockingPromoDisplayedAfterFRE,
     std::size(kIOSDockingPromoDisplayedAfterFRE), nullptr},
    {"Display promo at app launch", kIOSDockingPromoDisplayedAtAppLaunch,
     std::size(kIOSDockingPromoDisplayedAtAppLaunch), nullptr},
    {"Display promo during FRE", kIOSDockingPromoDisplayedDuringFRE,
     std::size(kIOSDockingPromoDisplayedDuringFRE), nullptr}};

const FeatureEntry::FeatureParam kModernTabStripNTBDynamic[] = {
    {kModernTabStripParameterName, kModernTabStripNTBDynamicParam}};
const FeatureEntry::FeatureParam kModernTabStripNTBStatic[] = {
    {kModernTabStripParameterName, kModernTabStripNTBStaticParam}};

const FeatureEntry::FeatureVariation kModernTabStripVariations[] = {
    {"New tab button dynamic", kModernTabStripNTBDynamic,
     std::size(kModernTabStripNTBDynamic), nullptr},
    {"New tab button static", kModernTabStripNTBStatic,
     std::size(kModernTabStripNTBStatic), nullptr},
};

const FeatureEntry::FeatureVariation
    kImageServiceOptimizationGuideSalientImagesVariations[] = {
        {"High Performance Canonicalization", nullptr, 0, "3362133"},
};

const FeatureEntry::FeatureParam kTabResumption15DisableSalientImages[] = {
    {kTR15SalientImageParam, "false"}};
const FeatureEntry::FeatureParam kTabResumption15DisableSeeMoreButtonImages[] =
    {{kTR15SeeMoreButtonParam, "false"}};
const FeatureEntry::FeatureParam kTabResumption15UIChangeOnlyImages[] = {
    {kTR15SeeMoreButtonParam, "false"},
    {kTR15SalientImageParam, "false"}};

const FeatureEntry::FeatureVariation kTabResumption15Variations[] = {
    {"No Salient Image", kTabResumption15DisableSalientImages,
     std::size(kTabResumption15DisableSalientImages), nullptr},
    {"No See More Button", kTabResumption15DisableSeeMoreButtonImages,
     std::size(kTabResumption15DisableSeeMoreButtonImages), nullptr},
    {"UI changes only", kTabResumption15UIChangeOnlyImages,
     std::size(kTabResumption15UIChangeOnlyImages), nullptr},
};
// Uses int values from Lens filters ablation mode enum.
const FeatureEntry::FeatureParam kLensFiltersAblationModeDisabled[] = {
    {kLensFiltersAblationMode, "0"}};
const FeatureEntry::FeatureParam kLensFiltersAblationModePostCapture[] = {
    {kLensFiltersAblationMode, "1"}};
const FeatureEntry::FeatureParam kLensFiltersAblationModeLVF[] = {
    {kLensFiltersAblationMode, "2"}};
const FeatureEntry::FeatureParam kLensFiltersAblationModeAlways[] = {
    {kLensFiltersAblationMode, "3"}};

const FeatureEntry::FeatureVariation kLensFiltersAblationModeVariations[] = {
    {"(Disabled)", kLensFiltersAblationModeDisabled,
     std::size(kLensFiltersAblationModeDisabled), nullptr},
    {"(Post Capture)", kLensFiltersAblationModePostCapture,
     std::size(kLensFiltersAblationModePostCapture), nullptr},
    {"(LVF)", kLensFiltersAblationModeLVF,
     std::size(kLensFiltersAblationModeLVF), nullptr},
    {"(Always)", kLensFiltersAblationModeAlways,
     std::size(kLensFiltersAblationModeAlways), nullptr}};

// Uses int values from Lens translate toggle mode enum.
const FeatureEntry::FeatureParam kLensTranslateToggleModeDisabled[] = {
    {kLensTranslateToggleMode, "0"}};
const FeatureEntry::FeatureParam kLensTranslateToggleModePostCapture[] = {
    {kLensTranslateToggleMode, "1"}};
const FeatureEntry::FeatureParam kLensTranslateToggleModeLVF[] = {
    {kLensTranslateToggleMode, "2"}};
const FeatureEntry::FeatureParam kLensTranslateToggleModeAlways[] = {
    {kLensTranslateToggleMode, "3"}};

const FeatureEntry::FeatureVariation kLensTranslateToggleModeVariations[] = {
    {"(Disabled)", kLensTranslateToggleModeDisabled,
     std::size(kLensTranslateToggleModeDisabled), nullptr},
    {"(Post Capture)", kLensTranslateToggleModePostCapture,
     std::size(kLensTranslateToggleModePostCapture), nullptr},
    {"(LVF)", kLensTranslateToggleModeLVF,
     std::size(kLensTranslateToggleModeLVF), nullptr},
    {"(Always)", kLensTranslateToggleModeAlways,
     std::size(kLensTranslateToggleModeAlways), nullptr}};

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
     FEATURE_WITH_PARAMS_VALUE_TYPE(kModernTabStrip,
                                    kModernTabStripVariations,
                                    "ModernTabStrip")},
    {"ios-shared-highlighting-color-change",
     flag_descriptions::kIOSSharedHighlightingColorChangeName,
     flag_descriptions::kIOSSharedHighlightingColorChangeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIOSSharedHighlightingColorChange)},
    {"ios-shared-highlighting-v2",
     flag_descriptions::kIOSSharedHighlightingV2Name,
     flag_descriptions::kIOSSharedHighlightingV2Description, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(shared_highlighting::kIOSSharedHighlightingV2)},
    {"ios-tips-notifications", flag_descriptions::kIOSTipsNotificationsName,
     flag_descriptions::kIOSTipsNotificationsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSTipsNotifications,
                                    kIOSTipsNotificationsVariations,
                                    "IOSTipsNotifications")},
    {"omnibox-new-textfield-implementation",
     flag_descriptions::kOmniboxNewImplementationName,
     flag_descriptions::kOmniboxNewImplementationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSNewOmniboxImplementation)},
    {"start-surface", flag_descriptions::kStartSurfaceName,
     flag_descriptions::kStartSurfaceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kStartSurface,
                                    kStartSurfaceVariations,
                                    "StartSurface")},
    {"wait-threshold-seconds-for-capabilities-api",
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiName,
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kWaitThresholdMillisecondsForCapabilitiesApiChoices)},
    {"content-notification-experiment",
     flag_descriptions::kContentNotificationExperimentName,
     flag_descriptions::kContentNotificationExperimentDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kContentNotificationExperiment)},
    {"content-notification-provisional-ignore-conditions",
     flag_descriptions::kContentNotificationProvisionalIgnoreConditionsName,
     flag_descriptions::
         kContentNotificationProvisionalIgnoreConditionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kContentNotificationProvisionalIgnoreConditions)},
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
    {"enable-lens-overlay", flag_descriptions::kEnableLensOverlayName,
     flag_descriptions::kEnableLensOverlayDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLensOverlay)},
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
    {"track-by-default-mobile",
     commerce::flag_descriptions::kTrackByDefaultOnMobileName,
     commerce::flag_descriptions::kTrackByDefaultOnMobileDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(commerce::kTrackByDefaultOnMobile)},
    {"web-feed-ios", flag_descriptions::kEnableWebChannelsName,
     flag_descriptions::kEnableWebChannelsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableWebChannels)},
    {"ntp-view-hierarchy-repair",
     flag_descriptions::kNTPViewHierarchyRepairName,
     flag_descriptions::kNTPViewHierarchyRepairDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableNTPViewHierarchyRepair)},
    {"unified-bookmark-model", flag_descriptions::kUnifiedBookmarkModelName,
     flag_descriptions::kUnifiedBookmarkModelDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kEnableBookmarkFoldersForAccountStorage)},
    {"price-insights", commerce::flag_descriptions::kPriceInsightsName,
     commerce::flag_descriptions::kPriceInsightsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kPriceInsights)},
    {"price-insights-ios", commerce::flag_descriptions::kPriceInsightsIosName,
     commerce::flag_descriptions::kPriceInsightsIosDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kPriceInsightsIos,
                                    kPriceInsightsVariations,
                                    "PriceInsightsIos")},
    {"price-insights-high-price-ios",
     commerce::flag_descriptions::kPriceInsightsHighPriceIosName,
     commerce::flag_descriptions::kPriceInsightsHighPriceIosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kPriceInsightsHighPriceIos)},
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
    {"intents-on-measurements", flag_descriptions::kMeasurementsName,
     flag_descriptions::kMeasurementsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableMeasurements)},
    {"intents-on-viewport", flag_descriptions::kEnableViewportIntentsName,
     flag_descriptions::kEnableViewportIntentsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableViewportIntents)},
    {"improve-parcel-detection",
     flag_descriptions::kEnableNewParcelTrackingNumberDetectionName,
     flag_descriptions::kEnableNewParcelTrackingNumberDetectionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         web::features::kEnableNewParcelTrackingNumberDetection)},
    {"enable-expkit-text-classifier-date",
     flag_descriptions::kEnableExpKitTextClassifierDateName,
     flag_descriptions::kEnableExpKitTextClassifierDateDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableExpKitTextClassifierDate)},
    {"enable-expkit-text-classifier-address",
     flag_descriptions::kEnableExpKitTextClassifierAddressName,
     flag_descriptions::kEnableExpKitTextClassifierAddressDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kEnableExpKitTextClassifierAddress,
         kEnableExpKitTextClassifierAddressVariations,
         "ExpKitTextClassifierAddress")},
    {"enable-expkit-text-classifier-phonenumber",
     flag_descriptions::kEnableExpKitTextClassifierPhoneNumberName,
     flag_descriptions::kEnableExpKitTextClassifierPhoneNumberDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kEnableExpKitTextClassifierPhoneNumber,
         kEnableExpKitTextClassifierPhoneNumberVariations,
         "ExpKitTextClassifierPhoneNumber")},
    {"enable-expkit-text-classifier-email",
     flag_descriptions::kEnableExpKitTextClassifierEmailName,
     flag_descriptions::kEnableExpKitTextClassifierEmailDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableExpKitTextClassifierEmail,
                                    kEnableExpKitTextClassifierEmailVariations,
                                    "ExpKitTextClassifierEmail")},
    {"one-tap-experience-maps", flag_descriptions::kOneTapForMapsName,
     flag_descriptions::kOneTapForMapsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(web::features::kOneTapForMaps,
                                    kOneTapForMapsWithVariations,
                                    "OneTapForMaps")},
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
    {"ios-magic-stack-segmentation-ranking-caching",
     flag_descriptions::kSegmentationPlatformIosModuleRankerCachingName,
     flag_descriptions::kSegmentationPlatformIosModuleRankerCachingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSegmentationPlatformIosModuleRankerCaching)},
    {"ios-magic-stack-segmentation-ranking-split-by-surface",
     flag_descriptions::kSegmentationPlatformIosModuleRankerSplitBySurfaceName,
     flag_descriptions::
         kSegmentationPlatformIosModuleRankerSplitBySurfaceDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         segmentation_platform::features::
             kSegmentationPlatformIosModuleRankerSplitBySurface)},
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
    {"sync-webauthn-credentials",
     flag_descriptions::kSyncWebauthnCredentialsName,
     flag_descriptions::kSyncWebauthnCredentialsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncWebauthnCredentials)},
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
    {"omnibox-max-url-matches", flag_descriptions::kOmniboxMaxURLMatchesName,
     flag_descriptions::kOmniboxMaxURLMatchesDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kOmniboxMaxURLMatches,
                                    kOmniboxMaxURLMatchesVariations,
                                    "OmniboxMaxURLMatches")},
    {"omnibox-most-visited-tiles-horizontal-render-group",
     flag_descriptions::kMostVisitedTilesHorizontalRenderGroupName,
     flag_descriptions::kMostVisitedTilesHorizontalRenderGroupDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kMostVisitedTilesHorizontalRenderGroup)},
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
    {"default-browser-promo-trigger-criteria-experiment",
     flag_descriptions::kDefaultBrowserTriggerCriteriaExperimentName,
     flag_descriptions::kDefaultBrowserTriggerCriteriaExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         feature_engagement::kDefaultBrowserTriggerCriteriaExperiment)},
    {"default-browser-video-in-settings",
     flag_descriptions::kDefaultBrowserVideoInSettingsName,
     flag_descriptions::kDefaultBrowserVideoInSettingsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDefaultBrowserVideoInSettings)},
#if BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"feed-background-refresh-ios",
     flag_descriptions::kFeedBackgroundRefreshName,
     flag_descriptions::kFeedBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFeedBackgroundRefresh,
                                    kFeedBackgroundRefreshVariations,
                                    "FeedBackgroundRefresh")},
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"enable-tflite-language-detection-ignore",
     flag_descriptions::kTFLiteLanguageDetectionIgnoreName,
     flag_descriptions::kTFLiteLanguageDetectionIgnoreDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(translate::kTFLiteLanguageDetectionIgnoreEnabled)},
    {"tab-grid-new-transitions", flag_descriptions::kTabGridNewTransitionsName,
     flag_descriptions::kTabGridNewTransitionsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridNewTransitions)},
    {"iph-price-notifications-while-browsing",
     flag_descriptions::kIPHPriceNotificationsWhileBrowsingName,
     flag_descriptions::kIPHPriceNotificationsWhileBrowsingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         feature_engagement::kIPHPriceNotificationsWhileBrowsingFeature)},
    {"shopping-list", commerce::flag_descriptions::kShoppingListName,
     commerce::flag_descriptions::kShoppingListDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kShoppingList)},
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
    {"spotlight-reading-list-source",
     flag_descriptions::kSpotlightReadingListSourceName,
     flag_descriptions::kSpotlightReadingListSourceDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kSpotlightReadingListSource)},
    {"enable-policy-test-page-ios",
     flag_descriptions::kEnablePolicyTestPageName,
     flag_descriptions::kEnablePolicyTestPageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(policy::features::kEnablePolicyTestPage)},
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
    {"enable-follow-ui-update", flag_descriptions::kEnableFollowUIUpdateName,
     flag_descriptions::kEnableFollowUIUpdateDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFollowUIUpdate)},
    {"enable-preferences-account-storage",
     flag_descriptions::kEnablePreferencesAccountStorageName,
     flag_descriptions::kEnablePreferencesAccountStorageDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kEnablePreferencesAccountStorage)},
    {"ios-browser-edit-menu-metrics",
     flag_descriptions::kIOSBrowserEditMenuMetricsName,
     flag_descriptions::kIOSBrowserEditMenuMetricsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSBrowserEditMenuMetrics)},
    {"ios-docking-promo", flag_descriptions::kIOSDockingPromoName,
     flag_descriptions::kIOSDockingPromoDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSDockingPromo,
                                    kIOSDockingPromoVariations,
                                    "IOSDockingPromo")},
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
         session::features::kEnableSessionSerializationOptimizations)},
    {"only-access-clipboard-async",
     flag_descriptions::kOnlyAccessClipboardAsyncName,
     flag_descriptions::kOnlyAccessClipboardAsyncDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOnlyAccessClipboardAsync)},
    {"omnibox-improved-rtl-suggestions",
     flag_descriptions::kOmniboxSuggestionsRTLImprovementsName,
     flag_descriptions::kOmniboxSuggestionsRTLImprovementsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kOmniboxSuggestionsRTLImprovements)},
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
    {"enable-startup-latency-improvements",
     flag_descriptions::kEnableStartupImprovementsName,
     flag_descriptions::kEnableStartupImprovementsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableStartupImprovements)},
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
    {"spotlight-open-tabs-source",
     flag_descriptions::kSpotlightOpenTabsSourceName,
     flag_descriptions::kSpotlightOpenTabsSourceDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSpotlightOpenTabsSource)},
    {"spotlight-never-retain-index",
     flag_descriptions::kSpotlightNeverRetainIndexName,
     flag_descriptions::kSpotlightNeverRetainIndexDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSpotlightNeverRetainIndex)},
    {"spotlight-donate-new-intents",
     flag_descriptions::kSpotlightDonateNewIntentsName,
     flag_descriptions::kSpotlightDonateNewIntentsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSpotlightDonateNewIntents)},
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
    {"top-toolbar-theme-color", flag_descriptions::kThemeColorInTopToolbarName,
     flag_descriptions::kThemeColorInTopToolbarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kThemeColorInTopToolbar)},
    {"autofill-enable-virtual-cards",
     flag_descriptions::kAutofillEnableVirtualCardsName,
     flag_descriptions::kAutofillEnableVirtualCardsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableVirtualCards)},
    {"enable-feed-containment", flag_descriptions::kEnableFeedContainmentName,
     flag_descriptions::kEnableFeedContainmentDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFeedContainment)},
    {"privacy-guide-ios", flag_descriptions::kPrivacyGuideIosName,
     flag_descriptions::kPrivacyGuideIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPrivacyGuideIos)},
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
     FEATURE_VALUE_TYPE(password_manager::features::kSendPasswords)},
    {"omnibox-company-entity-icon-adjustment",
     flag_descriptions::kOmniboxCompanyEntityIconAdjustmentName,
     flag_descriptions::kOmniboxCompanyEntityIconAdjustmentDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kCompanyEntityIconAdjustment,
                                    kOmniboxCompanyEntityAdjustmentVariations,
                                    "OmniboxCompanyEntityAdjustment")},
    {"fullscreen-improvement", flag_descriptions::kFullscreenImprovementName,
     flag_descriptions::kFullscreenImprovementDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenImprovement)},
    {"tab-groups-in-grid", flag_descriptions::kTabGroupsInGridName,
     flag_descriptions::kTabGroupsInGridDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGroupsInGrid)},
    {"tab-groups-on-ipad", flag_descriptions::kTabGroupsIPadName,
     flag_descriptions::kTabGroupsIPadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGroupsIPad)},
    {"autofill-enable-dynamically-loading-fields-on-input",
     flag_descriptions::
         kAutofillEnableDynamicallyLoadingFieldsForAddressInputName,
     flag_descriptions::
         kAutofillEnableDynamicallyLoadingFieldsForAddressInputDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillDynamicallyLoadsFieldsForAddressInput)},
    {"password-manager-signin-uff",
     flag_descriptions::kIOSPasswordSignInUffName,
     flag_descriptions::kIOSPasswordSignInUffDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSPasswordSignInUff)},
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
    {"revamp-page-info-ios", flag_descriptions::kRevampPageInfoIosName,
     flag_descriptions::kRevampPageInfoIosDescription, flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kRevampPageInfoiOSChoices)},
    {"remove-old-web-state-restore",
     flag_descriptions::kRemoveOldWebStateRestoreName,
     flag_descriptions::kRemoveOldWebStateRestoreDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kRemoveOldWebStateRestoration)},
    {"share-in-web-context-menu-ios",
     flag_descriptions::kShareInWebContextMenuIOSName,
     flag_descriptions::kShareInWebContextMenuIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kShareInWebContextMenuIOS)},
    {"migrate-syncing-user-to-signed-in",
     flag_descriptions::kMigrateSyncingUserToSignedInName,
     flag_descriptions::kMigrateSyncingUserToSignedInDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kMigrateSyncingUserToSignedIn)},
    {"undo-migration-of-syncing-user-to-signed-in",
     flag_descriptions::kUndoMigrationOfSyncingUserToSignedInName,
     flag_descriptions::kUndoMigrationOfSyncingUserToSignedInDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kUndoMigrationOfSyncingUserToSignedIn)},
    {"https-upgrades-ios", flag_descriptions::kHttpsUpgradesName,
     flag_descriptions::kHttpsUpgradesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(security_interstitials::features::kHttpsUpgrades)},
    {"contextual-panel", flag_descriptions::kContextualPanelName,
     flag_descriptions::kContextualPanelDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kContextualPanel)},
    {"omnibox-popup-row-content-configuration",
     flag_descriptions::kOmniboxPopupRowContentConfigurationName,
     flag_descriptions::kOmniboxPopupRowContentConfigurationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOmniboxPopupRowContentConfiguration)},
    {"contextual-panel-force-show-entrypoint",
     flag_descriptions::kContextualPanelForceShowEntrypointName,
     flag_descriptions::kContextualPanelForceShowEntrypointDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kContextualPanelForceShowEntrypoint)},
    {"bottom-omnibox-promo-region-filter",
     flag_descriptions::kBottomOmniboxPromoRegionFilterName,
     flag_descriptions::kBottomOmniboxPromoRegionFilterDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kBottomOmniboxPromoRegionFilter)},
    {"enable-ipad-feed-ghost-cards",
     flag_descriptions::kEnableiPadFeedGhostCardsName,
     flag_descriptions::kEnableiPadFeedGhostCardsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableiPadFeedGhostCards)},
    {"tab-grid-always-bounce", flag_descriptions::kTabGridAlwaysBounceName,
     flag_descriptions::kTabGridAlwaysBounceDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGridAlwaysBounce)},
    {"omnibox-rich-autocompletion",
     flag_descriptions::kOmniboxRichAutocompletionName,
     flag_descriptions::kOmniboxRichAutocompletionDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kRichAutocompletion,
         kRichAutocompletionImplementationVariations,
         "RichAutocompletionImplementationVariations")},
    {"disable-lens-camera", flag_descriptions::kDisableLensCameraName,
     flag_descriptions::kDisableLensCameraDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDisableLensCamera)},
    {"omnibox-color-icons", flag_descriptions::kOmniboxColorIconsName,
     flag_descriptions::kOmniboxColorIconsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOmniboxColorIcons)},
    {"enable-color-lens-and-voice-icons-in-home-screen-widget",
     flag_descriptions::kEnableColorLensAndVoiceIconsInHomeScreenWidgetName,
     flag_descriptions::
         kEnableColorLensAndVoiceIconsInHomeScreenWidgetDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableColorLensAndVoiceIconsInHomeScreenWidget)},
    {"minor-mode-restrictions-for-history-sync-opt-in",
     flag_descriptions::kMinorModeRestrictionsForHistorySyncOptInName,
     flag_descriptions::kMinorModeRestrictionsForHistorySyncOptInDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(::switches::kMinorModeRestrictionsForHistorySyncOptIn)},
    {"ios-iph-pull-to-refresh",
     flag_descriptions::kIPHiOSPullToRefreshFeatureName,
     flag_descriptions::kIPHiOSPullToRefreshFeatureDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHiOSPullToRefreshFeature)},
    {"ios-iph-swipe-back-forward",
     flag_descriptions::kIPHiOSSwipeBackForwardFeatureName,
     flag_descriptions::kIPHiOSSwipeBackForwardFeatureDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHiOSSwipeBackForwardFeature)},
    {"ios-iph-swipe-toolbar-to-change-tab",
     flag_descriptions::kIPHiOSSwipeToolbarToChangeTabFeatureName,
     flag_descriptions::kIPHiOSSwipeToolbarToChangeTabFeatureDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         feature_engagement::kIPHiOSSwipeToolbarToChangeTabFeature)},
    {"ios-magic-stack-collection-view",
     flag_descriptions::kIOSMagicStackCollectionViewName,
     flag_descriptions::kIOSMagicStackCollectionViewDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSMagicStackCollectionView)},
    {"autofill-enable-card-benefits-for-american-express",
     flag_descriptions::kAutofillEnableCardBenefitsForAmericanExpressName,
     flag_descriptions::
         kAutofillEnableCardBenefitsForAmericanExpressDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardBenefitsForAmericanExpress)},
    {"autofill-enable-card-benefits-for-capital-one",
     flag_descriptions::kAutofillEnableCardBenefitsForCapitalOneName,
     flag_descriptions::kAutofillEnableCardBenefitsForCapitalOneDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardBenefitsForCapitalOne)},
    {"autofill-enable-card-benefits-sync",
     flag_descriptions::kAutofillEnableCardBenefitsSyncName,
     flag_descriptions::kAutofillEnableCardBenefitsSyncDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardBenefitsSync)},
    {"linked-services-setting-ios",
     flag_descriptions::kLinkedServicesSettingIosName,
     flag_descriptions::kLinkedServicesSettingIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLinkedServicesSettingIos)},
    {"disable-fullscreen-scrolling",
     flag_descriptions::kDisableFullscreenScrollingName,
     flag_descriptions::kDisableFullscreenScrollingDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDisableFullscreenScrolling)},
    {"autofill-enable-xhr-submission-detection-ios",
     flag_descriptions::kAutofillEnableXHRSubmissionDetectionIOSName,
     flag_descriptions::kAutofillEnableXHRSubmissionDetectionIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableXHRSubmissionDetectionIOS)},
    {"autofill-enable-prefetching-risk-data-for-retrieval",
     flag_descriptions::kAutofillEnablePrefetchingRiskDataForRetrievalName,
     flag_descriptions::
         kAutofillEnablePrefetchingRiskDataForRetrievalDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnablePrefetchingRiskDataForRetrieval)},
    {"autofill-sticky-infobar", flag_descriptions::kAutofillStickyInfobarName,
     flag_descriptions::kAutofillStickyInfobarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillStickyInfobarIos)},
    {"page-content-annotations", flag_descriptions::kPageContentAnnotationsName,
     flag_descriptions::kPageContentAnnotationsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         page_content_annotations::features::kPageContentAnnotations)},
    {"page-content-annotations-persist-salient-image-metadata",
     flag_descriptions::kPageContentAnnotationsPersistSalientImageMetadataName,
     flag_descriptions::
         kPageContentAnnotationsPersistSalientImageMetadataDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         page_content_annotations::features::
             kPageContentAnnotationsPersistSalientImageMetadata)},
    {"page-content-annotations-remote-page-metadata",
     flag_descriptions::kPageContentAnnotationsRemotePageMetadataName,
     flag_descriptions::kPageContentAnnotationsRemotePageMetadataDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         page_content_annotations::features::kRemotePageMetadata)},
    {"page-visibility-page-content-annotations",
     flag_descriptions::kPageVisibilityPageContentAnnotationsName,
     flag_descriptions::kPageVisibilityPageContentAnnotationsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(page_content_annotations::features::
                            kPageVisibilityPageContentAnnotations)},
    {"enhanced-safe-browsing-promo",
     flag_descriptions::kEnhancedSafeBrowsingPromoName,
     flag_descriptions::kEnhancedSafeBrowsingPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(safe_browsing::kEnhancedSafeBrowsingPromo)},
    {"cpe-performance-improvements",
     flag_descriptions::kCredentialProviderPerformanceImprovementsName,
     flag_descriptions::kCredentialProviderPerformanceImprovementsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCredentialProviderPerformanceImprovements)},
    {"password-manager-detect-username-in-uff",
     flag_descriptions::kIOSDetectUsernameInUffName,
     flag_descriptions::kIOSPasswordSignInUffDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIosDetectUsernameInUff)},
    {"identity-disc-account-switch",
     flag_descriptions::kIdentityDiscAccountSwitchName,
     flag_descriptions::kIdentityDiscAccountSwitchDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIdentityDiscAccountSwitch)},
    {"ios-quick-delete", flag_descriptions::kIOSQuickDeleteName,
     flag_descriptions::kIOSQuickDeleteDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSQuickDelete)},
    {"autofill-enable-verve-card-support",
     flag_descriptions::kAutofillEnableVerveCardSupportName,
     flag_descriptions::kAutofillEnableVerveCardSupportDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableVerveCardSupport)},
    {"omnibox-actions-in-suggest",
     flag_descriptions::kOmniboxActionsInSuggestName,
     flag_descriptions::kOmniboxActionsInSuggestDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOmniboxActionsInSuggest)},
    {"tab-group-sync", flag_descriptions::kTabGroupSync,
     flag_descriptions::kTabGroupSync, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGroupSync)},
    {"autofill-enable-save-card-loading-and-confirmation",
     flag_descriptions::kAutofillEnableSaveCardLoadingAndConfirmationName,
     flag_descriptions::
         kAutofillEnableSaveCardLoadingAndConfirmationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSaveCardLoadingAndConfirmation)},
    {"autofill-enable-save-card-local-save-fallback",
     flag_descriptions::kAutofillEnableSaveCardLocalSaveFallbackName,
     flag_descriptions::kAutofillEnableSaveCardLocalSaveFallbackDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSaveCardLocalSaveFallback)},
    {"autofill-enable-vcn-enroll-loading-and-confirmation",
     flag_descriptions::kAutofillEnableVcnEnrollLoadingAndConfirmationName,
     flag_descriptions::
         kAutofillEnableVcnEnrollLoadingAndConfirmationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation)},
    {"lens-web-page-early-transition-enabled",
     flag_descriptions::kLensWebPageEarlyTransitionEnabledName,
     flag_descriptions::kLensWebPageEarlyTransitionEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensWebPageEarlyTransitionEnabled)},
    {"omnibox-suggestion-answer-migration",
     flag_descriptions::kOmniboxSuggestionAnswerMigrationName,
     flag_descriptions::kOmniboxSuggestionAnswerMigrationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox_feature_configs::SuggestionAnswerMigration::
                            kOmniboxSuggestionAnswerMigration)},
    {"tab-resumption1-5", flag_descriptions::kTabResumption1_5Name,
     flag_descriptions::kTabResumption1_5Description, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kTabResumption1_5,
                                    kTabResumption15Variations,
                                    "TabResumption1_5")},
    {"send-tab-ios-push-notifications",
     flag_descriptions::kSendTabToSelfIOSPushNotificationsName,
     flag_descriptions::kSendTabToSelfIOSPushNotificationsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfIOSPushNotifications)},
    {"tab-resumption-2", flag_descriptions::kTabResumption2Name,
     flag_descriptions::kTabResumption2Description, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabResumption2)},
    {"page-image-service-optimization-guide-salient-images",
     flag_descriptions::kPageImageServiceSalientImageName,
     flag_descriptions::kPageImageServiceSalientImageDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         page_image_service::kImageServiceOptimizationGuideSalientImages,
         kImageServiceOptimizationGuideSalientImagesVariations,
         "PageImageService")},
    {"downloaded-pdf-opening-ios", flag_descriptions::kDownloadedPDFOpeningName,
     flag_descriptions::kDownloadedPDFOpeningDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDownloadedPDFOpening)},
    {"clear-device-data-on-signout-for-managed-users",
     flag_descriptions::kClearDeviceDataOnSignOutForManagedUsersName,
     flag_descriptions::kClearDeviceDataOnSignOutForManagedUsersDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kClearDeviceDataOnSignOutForManagedUsers)},
    {"lens-filters-ablation-mode-enabled",
     flag_descriptions::kLensFiltersAblationModeEnabledName,
     flag_descriptions::kLensFiltersAblationModeEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kLensFiltersAblationModeEnabled,
                                    kLensFiltersAblationModeVariations,
                                    "LensFiltersAblationMode")},
    {"lens-translate-toogle-mode-enabled",
     flag_descriptions::kLensTranslateToggleModeEnabledName,
     flag_descriptions::kLensTranslateToggleModeEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kLensTranslateToggleModeEnabled,
                                    kLensTranslateToggleModeVariations,
                                    "LensTranslateToggleMode")},
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
