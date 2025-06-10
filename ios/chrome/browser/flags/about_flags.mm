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
#import "components/collaboration/public/features.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/flag_descriptions.h"
#import "components/content_settings/core/common/features.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/switches.h"
#import "components/dom_distiller/core/dom_distiller_switches.h"
#import "components/download/public/background_service/features.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feed/feed_feature_list.h"
#import "components/history/core/browser/features.h"
#import "components/invalidation/impl/invalidation_switches.h"
#import "components/ntp_tiles/features.h"
#import "components/ntp_tiles/switches.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/common/omnibox_feature_configs.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/page_content_annotations/core/page_content_annotations_features.h"
#import "components/page_image_service/features.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/payments/core/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/ios/browser/web_ui/features.h"
#import "components/search/ntp_features.cc"
#import "components/search_engines/search_engines_switches.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
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
#import "components/variations/net/variations_command_line.h"
#import "components/webui/flags/feature_entry.h"
#import "components/webui/flags/feature_entry_macros.h"
#import "components/webui/flags/flags_storage.h"
#import "components/webui/flags/flags_ui_switches.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_features.h"
#import "ios/chrome/browser/crash_report/model/features.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/download/ui/features.h"
#import "ios/chrome/browser/enterprise/connectors/features.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/lens/ui_bundled/features.h"
#import "ios/chrome/browser/ntp/model/features.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/page_info/ui_bundled/features.h"
#import "ios/chrome/browser/passwords/model/features.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/price_insights/model/price_insights_feature.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/screen_time/model/screen_time_buildflags.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/features.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_guide/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/text_selection/model/text_selection_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/tab_group_indicator_features_utils.h"
#import "ios/chrome/browser/web/model/features.h"
#import "ios/chrome/browser/whats_new/coordinator/whats_new_util.h"
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

const char kLensOverlayOnboardingParamSpeedbumpMenu[] =
    "kLensOverlayOnboardingParamSpeedbumpMenu";
const char kLensOverlayOnboardingParamUpdatedStrings[] =
    "kLensOverlayOnboardingParamUpdatedStrings";
const char kLensOverlayOnboardingParamUpdatedStringsAndVisuals[] =
    "kLensOverlayOnboardingParamUpdatedStringsAndVisuals";

const FeatureEntry::FeatureParam kLensOverlayOnboardinSpeedbumpMenu[] = {
    {kLensOverlayOnboardingParam, kLensOverlayOnboardingParamSpeedbumpMenu}};
const FeatureEntry::FeatureParam kLensOverlayOnboardingUpdatedStrings[] = {
    {kLensOverlayOnboardingParam, kLensOverlayOnboardingParamUpdatedStrings}};
const FeatureEntry::FeatureParam
    kLensOverlayOnboardingUpdatedStringsAndVisuals[] = {
        {kLensOverlayOnboardingParam,
         kLensOverlayOnboardingParamUpdatedStringsAndVisuals}};

const FeatureEntry::FeatureVariation kLensOverlayOnboardingVariations[] = {
    {"A: Speedbump menu", kLensOverlayOnboardinSpeedbumpMenu,
     std::size(kLensOverlayOnboardinSpeedbumpMenu), nullptr},
    {"B: Updated Strings", kLensOverlayOnboardingUpdatedStrings,
     std::size(kLensOverlayOnboardingUpdatedStrings), nullptr},
    {"C: Updated Strings and Graphics",
     kLensOverlayOnboardingUpdatedStringsAndVisuals,
     std::size(kLensOverlayOnboardingUpdatedStringsAndVisuals), nullptr},
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

const FeatureEntry::FeatureParam kEnableDefaultModel[] = {
    {segmentation_platform::kDefaultModelEnabledParam, "true"}};

const FeatureEntry::FeatureVariation
    kSegmentationPlatformIosModuleRankerVariations[]{
        {"Enabled With Default Model Parameter (Must Set this!)",
         kEnableDefaultModel, std::size(kEnableDefaultModel), nullptr},
    };

const FeatureEntry::FeatureParam
    kIOSReactivationNotifications10SecondTrigger[] = {
        {kIOSReactivationNotificationsTriggerTimeParam, "10s"},
};
const FeatureEntry::FeatureParam
    kIOSReactivationNotifications30SecondTrigger[] = {
        {kIOSReactivationNotificationsTriggerTimeParam, "30s"},
};
const FeatureEntry::FeatureVariation kIOSReactivationNotificationsVariations[] =
    {
        {"(10s trigger)", kIOSReactivationNotifications10SecondTrigger,
         std::size(kIOSReactivationNotifications10SecondTrigger), nullptr},
        {"(30s trigger)", kIOSReactivationNotifications30SecondTrigger,
         std::size(kIOSReactivationNotifications30SecondTrigger), nullptr},
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

const FeatureEntry::FeatureVariation
    kImageServiceOptimizationGuideSalientImagesVariations[] = {
        {"High Performance Canonicalization", nullptr, 0, "3362133"},
};

const FeatureEntry::FeatureParam kTabResumptionImagesOnlyThumbnail[] = {
    {kTabResumptionImagesTypes, kTabResumptionImagesTypesThumbnails}};
const FeatureEntry::FeatureParam kTabResumptionImagesOnlySalient[] = {
    {kTabResumptionImagesTypes, kTabResumptionImagesTypesSalient}};

const FeatureEntry::FeatureVariation kTabResumptionImagesVariations[] = {
    {"Only thumbnails", kTabResumptionImagesOnlyThumbnail,
     std::size(kTabResumptionImagesOnlyThumbnail), nullptr},
    {"Only salient", kTabResumptionImagesOnlySalient,
     std::size(kTabResumptionImagesOnlySalient), nullptr},
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

const FeatureEntry::FeatureParam kTabGroupIndicatorAboveButtonsVisble[] = {
    {kTabGroupIndicatorVisible, "true"},
    {kTabGroupIndicatorBelowOmnibox, "false"},
    {kTabGroupIndicatorButtonsUpdate, "true"}};
const FeatureEntry::FeatureParam kTabGroupIndicatorBelowButtonsVisble[] = {
    {kTabGroupIndicatorVisible, "true"},
    {kTabGroupIndicatorBelowOmnibox, "true"},
    {kTabGroupIndicatorButtonsUpdate, "true"}};
const FeatureEntry::FeatureParam kTabGroupIndicatorAboveVisble[] = {
    {kTabGroupIndicatorVisible, "true"},
    {kTabGroupIndicatorBelowOmnibox, "false"},
    {kTabGroupIndicatorButtonsUpdate, "false"}};
const FeatureEntry::FeatureParam kTabGroupIndicatorBelowVisble[] = {
    {kTabGroupIndicatorVisible, "true"},
    {kTabGroupIndicatorBelowOmnibox, "true"},
    {kTabGroupIndicatorButtonsUpdate, "false"}};
const FeatureEntry::FeatureParam kTabGroupIndicatorButtons[] = {
    {kTabGroupIndicatorVisible, "false"},
    {kTabGroupIndicatorBelowOmnibox, "false"},
    {kTabGroupIndicatorButtonsUpdate, "true"}};

const FeatureEntry::FeatureVariation kTabGroupIndicatorVariations[] = {
    {"Indicator above omnibox + buttons update",
     kTabGroupIndicatorAboveButtonsVisble,
     std::size(kTabGroupIndicatorAboveButtonsVisble), nullptr},
    {"Indicator below omnibox + buttons update",
     kTabGroupIndicatorBelowButtonsVisble,
     std::size(kTabGroupIndicatorBelowButtonsVisble), nullptr},
    {"Indicator above omnibox", kTabGroupIndicatorAboveVisble,
     std::size(kTabGroupIndicatorAboveVisble), nullptr},
    {"Indicator below omnibox", kTabGroupIndicatorBelowVisble,
     std::size(kTabGroupIndicatorBelowVisble), nullptr},
    {"buttons update only", kTabGroupIndicatorButtons,
     std::size(kTabGroupIndicatorButtons), nullptr}};

const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1300;0.14,1398;1,1422"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1400"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingDemotedBy50[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1250;0.14,1348;1,1422"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1350"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingPromotedBy50[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1350;0.14,1448;1,1472"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1450"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingPromotedBy100[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1400;0.14,1498;1,1522"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1500"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
const FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingMobileMapping[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,590;0.006,790;0.082,1290;0.443,1360;0.464,1400;0.987,1425;1,1530"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1340"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};

const FeatureEntry::FeatureVariation
    kMlUrlPiecewiseMappedSearchBlendingVariations[] = {
        {"adjusted by 0", kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0,
         std::size(kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0), nullptr},
        {"demoted by 50", kMlUrlPiecewiseMappedSearchBlendingDemotedBy50,
         std::size(kMlUrlPiecewiseMappedSearchBlendingDemotedBy50), nullptr},
        {"promoted by 50", kMlUrlPiecewiseMappedSearchBlendingPromotedBy50,
         std::size(kMlUrlPiecewiseMappedSearchBlendingPromotedBy50), nullptr},
        {"promoted by 100", kMlUrlPiecewiseMappedSearchBlendingPromotedBy100,
         std::size(kMlUrlPiecewiseMappedSearchBlendingPromotedBy100), nullptr},
        {"mobile mapping", kMlUrlPiecewiseMappedSearchBlendingMobileMapping,
         std::size(kMlUrlPiecewiseMappedSearchBlendingMobileMapping), nullptr},
};

const FeatureEntry::FeatureParam kOmniboxMiaZpsEnabledWithHistoryAblation[] = {
    {OmniboxFieldTrial::kSuppressPsuggestBackfillWithMIAParam, "true"}};
const FeatureEntry::FeatureVariation kOmniboxMiaZpsVariations[] = {
    {"with History Ablation", kOmniboxMiaZpsEnabledWithHistoryAblation,
     std::size(kOmniboxMiaZpsEnabledWithHistoryAblation), nullptr}};

const FeatureEntry::FeatureParam kOmniboxMlUrlScoringEnabledWithFixes[] = {
    {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
    {"MlUrlScoringShortcutDocumentSignals", "true"},
};
const FeatureEntry::FeatureParam kOmniboxMlUrlScoringUnlimitedNumCandidates[] =
    {
        {"MlUrlScoringUnlimitedNumCandidates", "true"},
        {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
        {"MlUrlScoringShortcutDocumentSignals", "true"},
};
// Sets Bookmark(1), History Quick(4), History URL(8), Shortcuts(64),
// Document(512), and History Fuzzy(65536) providers max matches to 10.
const FeatureEntry::FeatureParam kOmniboxMlUrlScoringMaxMatchesByProvider10[] =
    {
        {"MlUrlScoringMaxMatchesByProvider",
         "1:10,4:10,8:10,64:10,512:10,65536:10"},
        {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
        {"MlUrlScoringShortcutDocumentSignals", "true"},
};

const FeatureEntry::FeatureVariation kOmniboxMlUrlScoringVariations[] = {
    {"Enabled with fixes", kOmniboxMlUrlScoringEnabledWithFixes,
     std::size(kOmniboxMlUrlScoringEnabledWithFixes), nullptr},
    {"unlimited suggestion candidates",
     kOmniboxMlUrlScoringUnlimitedNumCandidates,
     std::size(kOmniboxMlUrlScoringUnlimitedNumCandidates), nullptr},
    {"Increase provider max limit to 10",
     kOmniboxMlUrlScoringMaxMatchesByProvider10,
     std::size(kOmniboxMlUrlScoringMaxMatchesByProvider10), nullptr},
};

const FeatureEntry::FeatureParam kMlUrlSearchBlendingStable[] = {
    {"MlUrlSearchBlending_StableSearchBlending", "true"},
    {"MlUrlSearchBlending_MappedSearchBlending", "false"},
};
const FeatureEntry::FeatureParam kMlUrlSearchBlendingMappedConservativeUrls[] =
    {
        {"MlUrlSearchBlending_StableSearchBlending", "false"},
        {"MlUrlSearchBlending_MappedSearchBlending", "true"},
        {"MlUrlSearchBlending_MappedSearchBlendingMin", "0"},
        {"MlUrlSearchBlending_MappedSearchBlendingMax", "2000"},
        {"MlUrlSearchBlending_MappedSearchBlendingGroupingThreshold", "1000"},
};
const FeatureEntry::FeatureParam kMlUrlSearchBlendingMappedModerateUrls[] = {
    {"MlUrlSearchBlending_StableSearchBlending", "false"},
    {"MlUrlSearchBlending_MappedSearchBlending", "true"},
};
const FeatureEntry::FeatureParam kMlUrlSearchBlendingMappedAggressiveUrls[] = {
    {"MlUrlSearchBlending_StableSearchBlending", "false"},
    {"MlUrlSearchBlending_MappedSearchBlending", "true"},
    {"MlUrlSearchBlending_MappedSearchBlendingMin", "1000"},
    {"MlUrlSearchBlending_MappedSearchBlendingMax", "4000"},
    {"MlUrlSearchBlending_MappedSearchBlendingGroupingThreshold", "1500"},
};

const FeatureEntry::FeatureVariation kMlUrlSearchBlendingVariations[] = {
    {"Stable", kMlUrlSearchBlendingStable,
     std::size(kMlUrlSearchBlendingStable), nullptr},
    {"Mapped conservative urls", kMlUrlSearchBlendingMappedConservativeUrls,
     std::size(kMlUrlSearchBlendingMappedConservativeUrls), nullptr},
    {"Mapped moderate urls", kMlUrlSearchBlendingMappedModerateUrls,
     std::size(kMlUrlSearchBlendingMappedModerateUrls), nullptr},
    {"Mapped aggressive urls", kMlUrlSearchBlendingMappedAggressiveUrls,
     std::size(kMlUrlSearchBlendingMappedAggressiveUrls), nullptr},
};

const FeatureEntry::FeatureVariation kUrlScoringModelVariations[] = {
    {"Small model", nullptr, 0, "3379590"},
    {"Full model", nullptr, 0, "3380197"},
};

const FeatureEntry::FeatureParam kSafetyCheckNotificationsVerbose[] = {
    {kSafetyCheckNotificationsExperimentType, "0"}};
const FeatureEntry::FeatureParam kSafetyCheckNotificationsSuccinct[] = {
    {kSafetyCheckNotificationsExperimentType, "1"}};

const FeatureEntry::FeatureVariation kSafetyCheckNotificationsVariations[] = {
    {"Display multiple notifications at once", kSafetyCheckNotificationsVerbose,
     std::size(kSafetyCheckNotificationsVerbose), nullptr},
    {"Display one notification at a time", kSafetyCheckNotificationsSuccinct,
     std::size(kSafetyCheckNotificationsSuccinct), nullptr}};

const FeatureEntry::FeatureParam kSaveToPhotosContextMenuImprovement[] = {
    {kSaveToPhotosContextMenuImprovementParam, "true"},
    {kSaveToPhotosTitleImprovementParam, "false"},
    {kSaveToPhotosAccountDefaultChoiceImprovementParam, "false"},
};
const FeatureEntry::FeatureParam kSaveToPhotosTitleImprovement[] = {
    {kSaveToPhotosContextMenuImprovementParam, "false"},
    {kSaveToPhotosTitleImprovementParam, "true"},
    {kSaveToPhotosAccountDefaultChoiceImprovementParam, "false"},
};
const FeatureEntry::FeatureParam
    kSaveToPhotosAccountDefaultChoiceImprovement[] = {
        {kSaveToPhotosContextMenuImprovementParam, "false"},
        {kSaveToPhotosTitleImprovementParam, "false"},
        {kSaveToPhotosAccountDefaultChoiceImprovementParam, "true"},
};

const FeatureEntry::FeatureVariation kSaveToPhotosImprovementsVariations[] = {
    {"With Context Menu improvement Only", kSaveToPhotosContextMenuImprovement,
     std::size(kSaveToPhotosContextMenuImprovement), nullptr},
    {"With Title improvement Only", kSaveToPhotosTitleImprovement,
     std::size(kSaveToPhotosTitleImprovement), nullptr},
    {"With Account Default choice improvement Only",
     kSaveToPhotosAccountDefaultChoiceImprovement,
     std::size(kSaveToPhotosAccountDefaultChoiceImprovement), nullptr},
};

// LINT.IfChange(AutofillUploadCardRequestTimeouts)
const FeatureEntry::FeatureParam
    kAutofillUploadCardRequestTimeout_6Point5Seconds[] = {
        {"autofill_upload_card_request_timeout_milliseconds", "6500"}};
const FeatureEntry::FeatureParam kAutofillUploadCardRequestTimeout_7Seconds[] =
    {{"autofill_upload_card_request_timeout_milliseconds", "7000"}};
const FeatureEntry::FeatureParam kAutofillUploadCardRequestTimeout_9Seconds[] =
    {{"autofill_upload_card_request_timeout_milliseconds", "9000"}};
const FeatureEntry::FeatureVariation
    kAutofillUploadCardRequestTimeoutOptions[] = {
        {"6.5 seconds", kAutofillUploadCardRequestTimeout_6Point5Seconds,
         std::size(kAutofillUploadCardRequestTimeout_6Point5Seconds), nullptr},
        {"7 seconds", kAutofillUploadCardRequestTimeout_7Seconds,
         std::size(kAutofillUploadCardRequestTimeout_7Seconds), nullptr},
        {"9 seconds", kAutofillUploadCardRequestTimeout_9Seconds,
         std::size(kAutofillUploadCardRequestTimeout_9Seconds), nullptr}};
// LINT.ThenChange(/chrome/browser/about_flags.cc:AutofillUploadCardRequestTimeouts)

// LINT.IfChange(AutofillVcnEnrollRequestTimeouts)
const FeatureEntry::FeatureParam kAutofillVcnEnrollRequestTimeout_5Seconds[] = {
    {"autofill_vcn_enroll_request_timeout_milliseconds", "5000"}};
const FeatureEntry::FeatureParam
    kAutofillVcnEnrollRequestTimeout_7Point5Seconds[] = {
        {"autofill_vcn_enroll_request_timeout_milliseconds", "7500"}};
const FeatureEntry::FeatureParam kAutofillVcnEnrollRequestTimeout_10Seconds[] =
    {{"autofill_vcn_enroll_request_timeout_milliseconds", "10000"}};
const FeatureEntry::FeatureVariation kAutofillVcnEnrollRequestTimeoutOptions[] =
    {{"5 seconds", kAutofillVcnEnrollRequestTimeout_5Seconds,
      std::size(kAutofillVcnEnrollRequestTimeout_5Seconds), nullptr},
     {"7.5 seconds", kAutofillVcnEnrollRequestTimeout_7Point5Seconds,
      std::size(kAutofillVcnEnrollRequestTimeout_7Point5Seconds), nullptr},
     {"10 seconds", kAutofillVcnEnrollRequestTimeout_10Seconds,
      std::size(kAutofillVcnEnrollRequestTimeout_10Seconds), nullptr}};
// LINT.ThenChange(/chrome/browser/about_flags.cc:AutofillVcnEnrollRequestTimeouts)

// Contextual Panel flag variations.
const FeatureEntry::FeatureParam kContextualPanelRichIPHArms[] = {
    {"entrypoint-highlight-iph", "true"},
    {"entrypoint-rich-iph", "true"},
};
const FeatureEntry::FeatureParam kContextualPanelSmallIPHArm[] = {
    {"entrypoint-highlight-iph", "false"},
    {"entrypoint-rich-iph", "false"},
};
const FeatureEntry::FeatureParam
    kContextualPanelSmallIPHWithBlueHighlightArm[] = {
        {"entrypoint-highlight-iph", "true"},
        {"entrypoint-rich-iph", "false"},
};

const FeatureEntry::FeatureVariation kContextualPanelEntrypointArmVariations[] =
    {
        {"- Rich IPH", kContextualPanelRichIPHArms,
         std::size(kContextualPanelRichIPHArms), nullptr},
        {"- Small IPH, no blue highlight", kContextualPanelSmallIPHArm,
         std::size(kContextualPanelSmallIPHArm), nullptr},
        {"- Small IPH with blue highlight",
         kContextualPanelSmallIPHWithBlueHighlightArm,
         std::size(kContextualPanelSmallIPHWithBlueHighlightArm), nullptr},
};

const FeatureEntry::FeatureParam kIdentityDiscAccountMenuWithSettings[] = {
    {kShowSettingsInAccountMenuParam, "true"},
};
const FeatureEntry::FeatureVariation kIdentityDiscAccountMenuVariations[] = {
    {" - with settings button", kIdentityDiscAccountMenuWithSettings,
     std::size(kIdentityDiscAccountMenuWithSettings), nullptr},
};

const FeatureEntry::FeatureParam kIdentityConfirmationSnackbarTestingConfig[] =
    {{"IdentityConfirmationMinDisplayInterval1", "0"},
     {"IdentityConfirmationMinDisplayInterval2", "0"},
     {"IdentityConfirmationMinDisplayInterval3", "0"}};
const FeatureEntry::FeatureVariation
    kIdentityConfirmationSnackbarTestingVariations[] = {
        {" - for testing", kIdentityConfirmationSnackbarTestingConfig,
         std::size(kIdentityConfirmationSnackbarTestingConfig), nullptr}};

const FeatureEntry::FeatureParam kPriceTrackingPromoForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kPriceTrackingNotificationPromo},
};
const FeatureEntry::FeatureParam kPriceTrackingPromoForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kPriceTrackingNotificationPromo},
};

// ShopCard variants
const FeatureEntry::FeatureParam kPriceDropForTrackedProductsArm[] = {
    {"ShopCardVariant", "arm_1"},
};
const FeatureEntry::FeatureParam kReviewsArm[] = {
    {"ShopCardVariant", "arm_2"},
};
const FeatureEntry::FeatureParam kPriceDropOnTabArm[] = {
    {"ShopCardVariant", "arm_3"},
};
const FeatureEntry::FeatureParam kPriceTrackableProductOnTabArm[] = {
    {"ShopCardVariant", "arm_4"},
};
const FeatureEntry::FeatureParam kTabResumptionWithImpressionLimitsArm[] = {
    {"ShopCardVariant", "arm_5"},
};
const FeatureEntry::FeatureParam kPriceDropForTrackedProductsFront[] = {
    {"ShopCardVariant", "arm_1"},
    {"ShopCardPosition", "shop_card_front"},
};
const FeatureEntry::FeatureParam kReviewsFrontt[] = {
    {"ShopCardVariant", "arm_2"},
    {"ShopCardPosition", "shop_card_front"},
};
const FeatureEntry::FeatureParam kPriceDropOnTabFront[] = {
    {"ShopCardVariant", "arm_3"},
    {"ShopCardPosition", "shop_card_front"},
};
const FeatureEntry::FeatureParam kPriceTrackableProductOnTabFront[] = {
    {"ShopCardVariant", "arm_4"},
    {"ShopCardPosition", "shop_card_front"},
};
const FeatureEntry::FeatureParam kTabResumptionWithImpressionLimitsFront[] = {
    {"ShopCardVariant", "arm_5"},
    {"ShopCardPosition", "shop_card_front"},
};

// Address Bar Position
const FeatureEntry::FeatureParam kTipsAddressBarPositionForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kAddressBarPositionEphemeralModule},
};
const FeatureEntry::FeatureParam kTipsAddressBarPositionForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kAddressBarPositionEphemeralModule},
};

// Autofill Passwords
const FeatureEntry::FeatureParam kTipsAutofillPasswordsForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kAutofillPasswordsEphemeralModule},
};
const FeatureEntry::FeatureParam kTipsAutofillPasswordsForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kAutofillPasswordsEphemeralModule},
};

// Enhanced Safe Browsing
const FeatureEntry::FeatureParam kTipsEnhancedSafeBrowsingForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kEnhancedSafeBrowsingEphemeralModule},
};
const FeatureEntry::FeatureParam kTipsEnhancedSafeBrowsingForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kEnhancedSafeBrowsingEphemeralModule},
};

// Lens Search
const FeatureEntry::FeatureParam kTipsLensSearchForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kLensEphemeralModuleSearchVariation},
};
const FeatureEntry::FeatureParam kTipsLensSearchForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kLensEphemeralModuleSearchVariation},
};

// Lens Shop
const FeatureEntry::FeatureParam kTipsLensShopForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kLensEphemeralModuleShopVariation},
};
const FeatureEntry::FeatureParam kTipsLensShopForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kLensEphemeralModuleShopVariation},
};

// Lens Translate
const FeatureEntry::FeatureParam kTipsLensTranslateForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kLensEphemeralModuleTranslateVariation},
};
const FeatureEntry::FeatureParam kTipsLensTranslateForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kLensEphemeralModuleTranslateVariation},
};

// Save Passwords
const FeatureEntry::FeatureParam kTipsSavePasswordsForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kSavePasswordsEphemeralModule},
};
const FeatureEntry::FeatureParam kTipsSavePasswordsForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kSavePasswordsEphemeralModule},
};

const FeatureEntry::FeatureParam kSendTabPromoForceShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kSendTabNotificationPromo},
};
const FeatureEntry::FeatureParam kSendTabPromoForceHideArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceHideCardParam,
     segmentation_platform::kSendTabNotificationPromo},
};

// ShopCard experiment arms
const FeatureEntry::FeatureVariation kShopCardOverrideOptions[] = {
    {"Card 1 Price Drop", kPriceDropForTrackedProductsArm,
     std::size(kPriceDropForTrackedProductsArm), nullptr},
    {"Card 2 Reviews", kReviewsArm, std::size(kReviewsArm), nullptr},
    {"Card 3 Price Drop on Tab Resumption", kPriceDropOnTabArm,
     std::size(kPriceDropOnTabArm), nullptr},
    {"Card 4 Price Trackable on Tab Resumption", kPriceTrackableProductOnTabArm,
     std::size(kPriceTrackableProductOnTabArm), nullptr},
    {"Card 5 Tab Resumption with Impression Limits",
     kTabResumptionWithImpressionLimitsArm,
     std::size(kTabResumptionWithImpressionLimitsArm), nullptr},
    {"Card 1 Price Drop at front of magic stack",
     kPriceDropForTrackedProductsFront,
     std::size(kPriceDropForTrackedProductsFront), nullptr},
    {"Card 2 Reviews at front of magic stack", kReviewsFrontt,
     std::size(kReviewsFrontt), nullptr},
    {"Card 3 Price Drop on Tab Resumption at front of magic stack",
     kPriceDropOnTabFront, std::size(kPriceDropOnTabFront), nullptr},
    {"Card 4 Price Trackable on Tab Resumption at front of magic stack",
     kPriceTrackableProductOnTabFront,
     std::size(kPriceTrackableProductOnTabFront), nullptr},
    {"Card 5 Tab Resumption with Impression Limits at front of magic stack",
     kTabResumptionWithImpressionLimitsFront,
     std::size(kTabResumptionWithImpressionLimitsFront), nullptr},
};

const FeatureEntry::FeatureVariation kEphemeralCardRankerCardOverrideOptions[] =
    {
        {"- Force Show Price Tracking Notification",
         kPriceTrackingPromoForceShowArm,
         std::size(kPriceTrackingPromoForceShowArm), nullptr},
        {"- Force Hide Price Tracking Notification",
         kPriceTrackingPromoForceHideArm,
         std::size(kPriceTrackingPromoForceHideArm), nullptr},

        // Address Bar Position
        {"- Force Show Address Bar Position Tip",
         kTipsAddressBarPositionForceShowArm,
         std::size(kTipsAddressBarPositionForceShowArm), nullptr},
        {"- Force Hide Address Bar Position Tip",
         kTipsAddressBarPositionForceHideArm,
         std::size(kTipsAddressBarPositionForceHideArm), nullptr},

        // Autofill Passwords
        {"- Force Show Autofill Passwords Tip",
         kTipsAutofillPasswordsForceShowArm,
         std::size(kTipsAutofillPasswordsForceShowArm), nullptr},
        {"- Force Hide Autofill Passwords Tip",
         kTipsAutofillPasswordsForceHideArm,
         std::size(kTipsAutofillPasswordsForceHideArm), nullptr},

        // Enhanced Safe Browsing
        {"- Force Show Enhanced Safe Browsing Tip",
         kTipsEnhancedSafeBrowsingForceShowArm,
         std::size(kTipsEnhancedSafeBrowsingForceShowArm), nullptr},
        {"- Force Hide Enhanced Safe Browsing Tip",
         kTipsEnhancedSafeBrowsingForceHideArm,
         std::size(kTipsEnhancedSafeBrowsingForceHideArm), nullptr},

        // Lens Search
        {"- Force Show Lens Search Tip", kTipsLensSearchForceShowArm,
         std::size(kTipsLensSearchForceShowArm), nullptr},
        {"- Force Hide Lens Search Tip", kTipsLensSearchForceHideArm,
         std::size(kTipsLensSearchForceHideArm), nullptr},

        // Lens Shop
        {"- Force Show Lens Shop Tip", kTipsLensShopForceShowArm,
         std::size(kTipsLensShopForceShowArm), nullptr},
        {"- Force Hide Lens Shop Tip", kTipsLensShopForceHideArm,
         std::size(kTipsLensShopForceHideArm), nullptr},

        // Lens Translate
        {"- Force Show Lens Translate Tip", kTipsLensTranslateForceShowArm,
         std::size(kTipsLensTranslateForceShowArm), nullptr},
        {"- Force Hide Lens Translate Tip", kTipsLensTranslateForceHideArm,
         std::size(kTipsLensTranslateForceHideArm), nullptr},

        // Save Passwords
        {"- Force Show Save Passwords Tip", kTipsSavePasswordsForceShowArm,
         std::size(kTipsSavePasswordsForceShowArm), nullptr},
        {"- Force Hide Save Passwords Tip", kTipsSavePasswordsForceHideArm,
         std::size(kTipsSavePasswordsForceHideArm), nullptr},

        // Send Tab Promo.
        {"- Force Show Send Tab Promo", kSendTabPromoForceShowArm,
         std::size(kSendTabPromoForceShowArm), nullptr},
        {"- Force Hide Send Tab Promo", kSendTabPromoForceHideArm,
         std::size(kSendTabPromoForceHideArm), nullptr},
};

const FeatureEntry::FeatureParam
    kSendTabIOSPushNotificationsWithMagicStackCard[] = {
        {send_tab_to_self::kSendTabIOSPushNotificationsWithMagicStackCardParam,
         "true"}};
const FeatureEntry::FeatureParam kSendTabIOSPushNotificationsWithURLImage[] = {
    {send_tab_to_self::kSendTabIOSPushNotificationsURLImageParam, "true"}};
const FeatureEntry::FeatureParam
    kSendTabIOSPushNotificationsWithTabReminders[] = {
        {send_tab_to_self::kSendTabIOSPushNotificationsWithTabRemindersParam,
         "true"}};

const FeatureEntry::FeatureVariation kSendTabIOSPushNotificationsVariations[] =
    {
        {"With Magic Stack Card",
         kSendTabIOSPushNotificationsWithMagicStackCard,
         std::size(kSendTabIOSPushNotificationsWithMagicStackCard), nullptr},
        {"With URL Image", kSendTabIOSPushNotificationsWithURLImage,
         std::size(kSendTabIOSPushNotificationsWithURLImage), nullptr},
        {"With Tab Reminders", kSendTabIOSPushNotificationsWithTabReminders,
         std::size(kSendTabIOSPushNotificationsWithTabReminders), nullptr},
};

const FeatureEntry::FeatureParam kSegmentedDefaultBrowserStatic[] = {
    {kSegmentedDefaultBrowserExperimentType, "0"}};
const FeatureEntry::FeatureParam kSegmentedDefaultBrowserAnimated[] = {
    {kSegmentedDefaultBrowserExperimentType, "1"}};

const FeatureEntry::FeatureVariation kSegmentedDefaultBrowserPromoVariations[] =
    {{" - Static Default Browser Promo", kSegmentedDefaultBrowserStatic,
      std::size(kSegmentedDefaultBrowserStatic), nullptr},
     {" - Animated Default Browser Promo", kSegmentedDefaultBrowserAnimated,
      std::size(kSegmentedDefaultBrowserAnimated), nullptr}};

// Soft Lock
const FeatureEntry::FeatureParam kIOSSoftLockNoDelay[] = {
    {kIOSSoftLockBackgroundThresholdParam, "0m"},
};

const FeatureEntry::FeatureVariation kIOSSoftLockVariations[] = {
    {" - No delay", kIOSSoftLockNoDelay, std::size(kIOSSoftLockNoDelay),
     nullptr}};

// Ipad ZPS limit.
const FeatureEntry::FeatureParam kIpadZPSOmniboxWith20Total0Trends[] = {
    {OmniboxFieldTrial::kIpadAdditionalTrendingQueries.name, "0"},
    {OmniboxFieldTrial::kIpadZPSLimit.name, "20"},
};

const FeatureEntry::FeatureParam kIpadZPSOmniboxWith20Total5Trends[] = {
    {OmniboxFieldTrial::kIpadAdditionalTrendingQueries.name, "5"},
    {OmniboxFieldTrial::kIpadZPSLimit.name, "20"},
};

constexpr FeatureEntry::FeatureVariation kIpadZpsLimitVariants[] = {
    {"20 total zps, 0 Trends on NTP", kIpadZPSOmniboxWith20Total0Trends,
     std::size(kIpadZPSOmniboxWith20Total0Trends), nullptr},
    {"20 total zps, 5 Trends on NTP", kIpadZPSOmniboxWith20Total5Trends,
     std::size(kIpadZPSOmniboxWith20Total0Trends), nullptr},
};

const FeatureEntry::FeatureParam
    kIOSStartTimeStartupRemediationsSaveNTPWebStateArm[] = {
        {kIOSStartTimeStartupRemediationsSaveNTPWebState, "true"},
};
const FeatureEntry::FeatureVariation
    kIOSStartTimeStartupRemediationsVariations[] = {
        {" - Save NTP Web State",
         kIOSStartTimeStartupRemediationsSaveNTPWebStateArm,
         std::size(kIOSStartTimeStartupRemediationsSaveNTPWebStateArm),
         nullptr}};

constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanShortPeriodParam[] = {{"period-ms", "250"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanMediumPeriodParam[] = {{"period-ms", "500"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanLongPeriodParam[] = {{"period-ms", "1000"}};
constexpr flags_ui::FeatureEntry::FeatureVariation
    kAutofillThrottleDocFormScanVariations[] = {
        {"Short period", kAutofillThrottleDocFormScanShortPeriodParam,
         std::size(kAutofillThrottleDocFormScanShortPeriodParam), nullptr},
        {"Medium period", kAutofillThrottleDocFormScanMediumPeriodParam,
         std::size(kAutofillThrottleDocFormScanMediumPeriodParam), nullptr},
        {"Long period", kAutofillThrottleDocFormScanLongPeriodParam,
         std::size(kAutofillThrottleDocFormScanLongPeriodParam), nullptr}};

constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleFilteredDocFormScanShortPeriodParam[] = {
        {"period-ms", "100"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleFilteredDocFormScanMediumPeriodParam[] = {
        {"period-ms", "250"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleFilteredDocFormScanLongPeriodParam[] = {
        {"period-ms", "500"}};
constexpr flags_ui::FeatureEntry::FeatureVariation
    kAutofillThrottleFilteredDocFormScanVariations[] = {
        {"Short period", kAutofillThrottleFilteredDocFormScanShortPeriodParam,
         std::size(kAutofillThrottleFilteredDocFormScanShortPeriodParam),
         nullptr},
        {"Medium period", kAutofillThrottleFilteredDocFormScanMediumPeriodParam,
         std::size(kAutofillThrottleFilteredDocFormScanMediumPeriodParam),
         nullptr},
        {"Long period", kAutofillThrottleFilteredDocFormScanLongPeriodParam,
         std::size(kAutofillThrottleFilteredDocFormScanLongPeriodParam),
         nullptr}};

const FeatureEntry::FeatureParam
    kIOSStartTimeBackgroundRemediationsAvoidNTPCleanupArm[] = {
        {kIOSStartTimeBackgroundRemediationsAvoidNTPCleanup, "true"},
        {kIOSStartTimeBrowserBackgroundRemediationsUpdateFeedRefresh, "false"}};
const FeatureEntry::FeatureParam
    kIOSStartTimeBrowserBackgroundRemediationsUpdateFeedRefreshArm[] = {
        {kIOSStartTimeBrowserBackgroundRemediationsUpdateFeedRefresh, "true"},
        {kIOSStartTimeBackgroundRemediationsAvoidNTPCleanup, "false"}};
const FeatureEntry::FeatureVariation
    kIOSStartTimeBrowserBackgroundRemediationsVariations[] = {
        {" - Avoid NTP Cleanup",
         kIOSStartTimeBackgroundRemediationsAvoidNTPCleanupArm,
         std::size(kIOSStartTimeBackgroundRemediationsAvoidNTPCleanupArm),
         nullptr},
        {" - Update Feed Refresh",
         kIOSStartTimeBrowserBackgroundRemediationsUpdateFeedRefreshArm,
         std::size(
             kIOSStartTimeBrowserBackgroundRemediationsUpdateFeedRefreshArm),
         nullptr}};

const FeatureEntry::FeatureParam kSetUpListDuration3Days[] = {
    {set_up_list::kSetUpListDurationParam, "2"}};
const FeatureEntry::FeatureParam kSetUpListDuration5Days[] = {
    {set_up_list::kSetUpListDurationParam, "4"}};
const FeatureEntry::FeatureParam kSetUpListDuration7Days[] = {
    {set_up_list::kSetUpListDurationParam, "6"}};

const FeatureEntry::FeatureVariation kSetUpListDurationVariations[] = {
    {" - 3 Days", kSetUpListDuration3Days, std::size(kSetUpListDuration3Days),
     nullptr},
    {" - 5 Days", kSetUpListDuration5Days, std::size(kSetUpListDuration5Days),
     nullptr},
    {" - 7 Days", kSetUpListDuration7Days, std::size(kSetUpListDuration7Days),
     nullptr}};

const FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm1[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "1"}};
const FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm2[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "2"}};
const FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm3[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "3"}};

const FeatureEntry::FeatureVariation kUpdatedFirstRunSequenceVariations[] = {
    {" - Default browser promo first", kUpdatedFirstRunSequenceArm1,
     std::size(kUpdatedFirstRunSequenceArm1), nullptr},
    {" - Remove sign in & sync conditionally", kUpdatedFirstRunSequenceArm2,
     std::size(kUpdatedFirstRunSequenceArm2), nullptr},
    {" - DB promo first and remove sign in & sync",
     kUpdatedFirstRunSequenceArm3, std::size(kUpdatedFirstRunSequenceArm3),
     nullptr}};

const FeatureEntry::FeatureParam
    kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitial[] = {
        {kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitialParam,
         "true"},
};

const FeatureEntry::FeatureParam kYoutubeIncognitoTargetAllowListed[] = {
    {kYoutubeIncognitoTargetApps, kYoutubeIncognitoTargetAppsAllowlisted},
};
const FeatureEntry::FeatureParam kYoutubeIncognitoTargetFirstParty[] = {
    {kYoutubeIncognitoTargetApps, kYoutubeIncognitoTargetAppsFirstParty},
};
const FeatureEntry::FeatureParam kYoutubeIncognitoTargetAll[] = {
    {kYoutubeIncognitoTargetApps, kYoutubeIncognitoTargetAppsAll},
};

const FeatureEntry::FeatureVariation kYoutubeIncognitoVariations[] = {
    {"Error handling without Incognito Interstitial",
     kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitial,
     std::size(kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitial),
     nullptr},
    {"Enable for listed apps", kYoutubeIncognitoTargetAllowListed,
     std::size(kYoutubeIncognitoTargetAllowListed), nullptr},
    {"Enable for first party apps", kYoutubeIncognitoTargetFirstParty,
     std::size(kYoutubeIncognitoTargetFirstParty), nullptr},
    {"Enable for all apps", kYoutubeIncognitoTargetAll,
     std::size(kYoutubeIncognitoTargetAll), nullptr},
};

const FeatureEntry::FeatureParam kSlowFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "0"}};
const FeatureEntry::FeatureParam kDefaultFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "1"}};
const FeatureEntry::FeatureParam kFastFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "2"}};
const FeatureEntry::FeatureParam kMediumFullscreenTransitionOffset[] = {
    {kMediumFullscreenTransitionOffsetParam, "true"}};

const FeatureEntry::FeatureVariation kFullscreenTransitionVariations[] = {
    {"Slow speed", kSlowFullscreenTransitionSpeed,
     std::size(kSlowFullscreenTransitionSpeed), nullptr},
    {"Default speed", kDefaultFullscreenTransitionSpeed,
     std::size(kDefaultFullscreenTransitionSpeed), nullptr},
    {"Fast speed", kFastFullscreenTransitionSpeed,
     std::size(kFastFullscreenTransitionSpeed), nullptr},
    {"Medium offset", kMediumFullscreenTransitionOffset,
     std::size(kMediumFullscreenTransitionOffset), nullptr}};

const FeatureEntry::FeatureParam
    kDeprecateFeedHeaderVariationRemoveFeedLabel[] = {
        {kDeprecateFeedHeaderParameterRemoveLabel, "true"}};
const FeatureEntry::FeatureParam
    kDeprecateFeedHeaderVariationAbovePlusMoreSpacing[] = {
        {kDeprecateFeedHeaderParameterRemoveLabel, "true"},
        {kDeprecateFeedHeaderParameterTopPadding, "28.5"},
        {kDeprecateFeedHeaderParameterSearchFieldTopMargin, "30"},
        {kDeprecateFeedHeaderParameterHeaderBottomPadding, "3"},
        {kDeprecateFeedHeaderParameterSpaceBetweenModules, "16"}};
const FeatureEntry::FeatureParam
    kDeprecateFeedHeaderVariationAbovePlusEnlargeElements[] = {
        {kDeprecateFeedHeaderParameterRemoveLabel, "true"},
        {kDeprecateFeedHeaderParameterTopPadding, "21.5"},
        {kDeprecateFeedHeaderParameterSearchFieldTopMargin, "32"},
        {kDeprecateFeedHeaderParameterEnlargeLogoAndFakebox, "true"}};

const FeatureEntry::FeatureVariation kDeprecateFeedHeaderVariations[] = {
    {" (remove feed label)", kDeprecateFeedHeaderVariationRemoveFeedLabel,
     std::size(kDeprecateFeedHeaderVariationRemoveFeedLabel), nullptr},
    {" (also add more spacing)",
     kDeprecateFeedHeaderVariationAbovePlusMoreSpacing,
     std::size(kDeprecateFeedHeaderVariationAbovePlusMoreSpacing), nullptr},
    {" (and enlarge doodle too)",
     kDeprecateFeedHeaderVariationAbovePlusEnlargeElements,
     std::size(kDeprecateFeedHeaderVariationAbovePlusEnlargeElements),
     nullptr}};

const FeatureEntry::FeatureParam kAnimatedDBPInFREWithActionButtons[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "0"}};
const FeatureEntry::FeatureParam kAnimatedDBPInFREWithShowMeHow[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "1"}};
const FeatureEntry::FeatureParam kAnimatedDBPInFREWithInstructions[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "2"}};

const FeatureEntry::FeatureVariation
    kAnimatedDefaultBrowserPromoInFREVariations[] = {
        {" - with Action Buttons", kAnimatedDBPInFREWithActionButtons,
         std::size(kAnimatedDBPInFREWithActionButtons), nullptr},
        {" - with Show Me How", kAnimatedDBPInFREWithShowMeHow,
         std::size(kAnimatedDBPInFREWithShowMeHow), nullptr},
        {" - with Instructions", kAnimatedDBPInFREWithInstructions,
         std::size(kAnimatedDBPInFREWithInstructions), nullptr}};

const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm1[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "1"}};
const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm2[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "2"}};
const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm3[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "3"}};
const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm4[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "4"}};
const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm5[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "5"}};
const FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm6[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "6"}};

const FeatureEntry::FeatureVariation kBestFeaturesScreenInFirstRunVariations[] =
    {{" - Variant A: General screen, after DB promo",
      kUpdatedFirstRunSequenceArm1,
      std::size(kBestFeaturesScreenInFirstRunArm1), nullptr},
     {" - Variant B: General screen, before DB promo",
      kBestFeaturesScreenInFirstRunArm2,
      std::size(kBestFeaturesScreenInFirstRunArm2), nullptr},
     {" - Variant C: General screen with passwords item",
      kBestFeaturesScreenInFirstRunArm3,
      std::size(kBestFeaturesScreenInFirstRunArm3), nullptr},
     {" - Variant D: Shopping users screen, variant C as fallback",
      kBestFeaturesScreenInFirstRunArm4,
      std::size(kBestFeaturesScreenInFirstRunArm4), nullptr},
     {" - Variant E: Show screen to signed-in users only",
      kBestFeaturesScreenInFirstRunArm5,
      std::size(kBestFeaturesScreenInFirstRunArm5), nullptr},
     {" - Variant F: Show address bar promo", kBestFeaturesScreenInFirstRunArm6,
      std::size(kBestFeaturesScreenInFirstRunArm6), nullptr}};

const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionCrossValidate[] = {
    {kIOSOneTapMiniMapRestrictionCrossValidateParamName, "true"}};
const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionThreshold999[] = {
    {kIOSOneTapMiniMapRestrictionThreshholdParamName, "0.999"}};
const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionMinLength20[] = {
    {kIOSOneTapMiniMapRestrictionMinCharsParamName, "20"}};
const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionMaxSections6[] = {
    {kIOSOneTapMiniMapRestrictionMaxSectionsParamName, "6"}};
const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionLongWords4[] = {
    {kIOSOneTapMiniMapRestrictionLongestWordMinCharsParamName, "4"}};
const FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionMinAlphaNum60[] = {
    {kIOSOneTapMiniMapRestrictionMinAlphanumProportionParamName, "0.6"}};

const FeatureEntry::FeatureVariation kIOSOneTapMiniMapRestrictionsVariations[] =
    {{"Revalidate with NSDataDetector",
      kIOSOneTapMiniMapRestrictionCrossValidate,
      std::size(kIOSOneTapMiniMapRestrictionCrossValidate), nullptr},
     {"Confidence Level (0.999)", kIOSOneTapMiniMapRestrictionThreshold999,
      std::size(kIOSOneTapMiniMapRestrictionThreshold999), nullptr},
     {"Minimum address length (20 chars)",
      kIOSOneTapMiniMapRestrictionMinLength20,
      std::size(kIOSOneTapMiniMapRestrictionMinLength20), nullptr},
     {"Maximum sections (6)", kIOSOneTapMiniMapRestrictionMaxSections6,
      std::size(kIOSOneTapMiniMapRestrictionMaxSections6), nullptr},
     {"Longest word length (4)", kIOSOneTapMiniMapRestrictionLongWords4,
      std::size(kIOSOneTapMiniMapRestrictionLongWords4), nullptr},
     {"Proportion of alnum chars (60%)",
      kIOSOneTapMiniMapRestrictionMinAlphaNum60,
      std::size(kIOSOneTapMiniMapRestrictionMinAlphaNum60), nullptr}};

const FeatureEntry::FeatureParam kFeedSwipeInProductHelpStaticInFirstRun[] = {
    {kFeedSwipeInProductHelpArmParam, "1"}};
const FeatureEntry::FeatureParam kFeedSwipeInProductHelpStaticInSecondRun[] = {
    {kFeedSwipeInProductHelpArmParam, "2"}};
const FeatureEntry::FeatureParam kFeedSwipeInProductHelpAnimated[] = {
    {kFeedSwipeInProductHelpArmParam, "3"}};

const FeatureEntry::FeatureVariation kFeedSwipeInProductHelpVariations[] = {
    {" - Static IPH after the FRE", kFeedSwipeInProductHelpStaticInFirstRun,
     std::size(kFeedSwipeInProductHelpStaticInFirstRun), nullptr},
    {"- Static IPH after the second run",
     kFeedSwipeInProductHelpStaticInSecondRun,
     std::size(kFeedSwipeInProductHelpStaticInSecondRun), nullptr},
    {"- Animated IPH", kFeedSwipeInProductHelpAnimated,
     std::size(kFeedSwipeInProductHelpAnimated), nullptr}};

// LINT.IfChange(AutofillVcnEnrollStrikeExpiryTime)
const FeatureEntry::FeatureParam kAutofillVcnEnrollStrikeExpiryTime_120Days[] =
    {{"autofill_vcn_strike_expiry_time_days", "120"}};

const FeatureEntry::FeatureParam kAutofillVcnEnrollStrikeExpiryTime_60Days[] = {
    {"autofill_vcn_strike_expiry_time_days", "60"}};

const FeatureEntry::FeatureParam kAutofillVcnEnrollStrikeExpiryTime_30Days[] = {
    {"autofill_vcn_strike_expiry_time_days", "30"}};

const FeatureEntry::FeatureVariation
    kAutofillVcnEnrollStrikeExpiryTimeOptions[] = {
        {"120 days", kAutofillVcnEnrollStrikeExpiryTime_120Days,
         std::size(kAutofillVcnEnrollStrikeExpiryTime_120Days), nullptr},
        {"60 days", kAutofillVcnEnrollStrikeExpiryTime_60Days,
         std::size(kAutofillVcnEnrollStrikeExpiryTime_60Days), nullptr},
        {"30 days", kAutofillVcnEnrollStrikeExpiryTime_30Days,
         std::size(kAutofillVcnEnrollStrikeExpiryTime_30Days), nullptr}};
// LINT.ThenChange(/chrome/browser/about_flags.cc:AutofillVcnEnrollStrikeExpiryTime)

const FeatureEntry::FeatureParam kWelcomeBackInFirstRunArm1[] = {
    {first_run::kWelcomeBackInFirstRunParam, "1"}};
const FeatureEntry::FeatureParam kWelcomeBackInFirstRunArm2[] = {
    {first_run::kWelcomeBackInFirstRunParam, "2"}};
const FeatureEntry::FeatureParam kWelcomeBackInFirstRunArm3[] = {
    {first_run::kWelcomeBackInFirstRunParam, "3"}};
const FeatureEntry::FeatureParam kWelcomeBackInFirstRunArm4[] = {
    {first_run::kWelcomeBackInFirstRunParam, "4"}};

const FeatureEntry::FeatureVariation kWelcomeBackInFirstRunVariations[] = {
    {" - Variant A: Basics with Locked Incognito", kWelcomeBackInFirstRunArm1,
     std::size(kWelcomeBackInFirstRunArm1), nullptr},
    {" - Variant B: Basics with Save & Autofill Passwords",
     kWelcomeBackInFirstRunArm2, std::size(kWelcomeBackInFirstRunArm2),
     nullptr},
    {" - Variant C: Productivity & Shopping", kWelcomeBackInFirstRunArm3,
     std::size(kWelcomeBackInFirstRunArm3), nullptr},
    {" - Variant D: Sign-in Benefits", kWelcomeBackInFirstRunArm4,
     std::size(kWelcomeBackInFirstRunArm4), nullptr},
};

const FeatureEntry::FeatureParam kBestOfAppFREArm1[] = {{"variant", "1"}};
const FeatureEntry::FeatureParam kBestOfAppFREArm4[] = {{"variant", "4"}};

const FeatureEntry::FeatureVariation kBestOfAppFREVariations[] = {
    {" - Variant A: Lens Interactive Promo", kBestOfAppFREArm1,
     std::size(kWelcomeBackInFirstRunArm1), nullptr},
    {" - Variant D: Guided Tour", kBestOfAppFREArm4,
     std::size(kWelcomeBackInFirstRunArm4), nullptr},
};

const FeatureEntry::FeatureParam
    kInvalidateChoiceOnRestoreIsRetroactiveOption[] = {
        {"is_retroactive", "true"}};
const FeatureEntry::FeatureVariation
    kInvalidateSearchEngineChoiceOnRestoreVariations[] = {
        {"(retroactive)", kInvalidateChoiceOnRestoreIsRetroactiveOption,
         std::size(kInvalidateChoiceOnRestoreIsRetroactiveOption), nullptr}};

const FeatureEntry::FeatureParam kSingleScreenForGLICPromoConsent[] = {
    {kGLICPromoConsentParams, "1"}};
const FeatureEntry::FeatureParam kDoubleScreenForGLICPromoConsent[] = {
    {kGLICPromoConsentParams, "2"}};
const FeatureEntry::FeatureParam kSkipGLICPromoConsent[] = {
    {kGLICPromoConsentParams, "3"}};

const FeatureEntry::FeatureVariation kGLICPromoConsentVariations[] = {
    {"Single screen for GLIC Promo Consent Flow",
     kSingleScreenForGLICPromoConsent,
     std::size(kSingleScreenForGLICPromoConsent), nullptr},
    {"Double screen for GLIC Promo Consent Flow",
     kDoubleScreenForGLICPromoConsent,
     std::size(kDoubleScreenForGLICPromoConsent), nullptr},
    {"Skip FRE", kSkipGLICPromoConsent, std::size(kSkipGLICPromoConsent),
     nullptr}};

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
    {"sign-in-button-no-avatar", flag_descriptions::kSignInButtonNoAvatarName,
     flag_descriptions::kSignInButtonNoAvatarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSignInButtonNoAvatar)},
    {"ntp-background-customization",
     flag_descriptions::kNTPBackgroundCustomizationName,
     flag_descriptions::kNTPBackgroundCustomizationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNTPBackgroundCustomization)},
    {"ntp-alpha-background-collections",
     flag_descriptions::kNtpAlphaBackgroundCollectionsName,
     flag_descriptions::kNtpAlphaBackgroundCollectionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(ntp_features::kNtpAlphaBackgroundCollections)},
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
    {"omnibox-ipad-zps-limit",
     flag_descriptions::kIpadZpsSuggestionMatchesLimitName,
     flag_descriptions::kIpadZpsSuggestionMatchesLimitDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kIpadZeroSuggestMatches,
                                    kIpadZpsLimitVariants,
                                    "OmniboxBundledExperimentV1")},
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
    {"omnibox-mobile-parity-update",
     flag_descriptions::kOmniboxMobileParityUpdateName,
     flag_descriptions::kOmniboxMobileParityUpdateDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxMobileParityUpdate)},
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
    {"ios-enable-delete-all-saved-credentials",
     flag_descriptions::kIOSEnableDeleteAllSavedCredentialsName,
     flag_descriptions::kIOSEnableDeleteAllSavedCredentialsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kIOSEnableDeleteAllSavedCredentials)},
    {"ios-shared-highlighting-color-change",
     flag_descriptions::kIOSSharedHighlightingColorChangeName,
     flag_descriptions::kIOSSharedHighlightingColorChangeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIOSSharedHighlightingColorChange)},
    {"ios-reactivation-notifications",
     flag_descriptions::kIOSReactivationNotificationsName,
     flag_descriptions::kIOSReactivationNotificationsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSReactivationNotifications,
                                    kIOSReactivationNotificationsVariations,
                                    "IOSReactivationNotifications")},
    {"ios-expanded-tips", flag_descriptions::kIOSExpandedTipsName,
     flag_descriptions::kIOSExpandedTipsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSExpandedTips)},
    {"invalidate-search-engine-choice-on-device-restore-detection",
     flag_descriptions::
         kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionName,
     flag_descriptions::
         kInvalidateSearchEngineChoiceOnDeviceRestoreDetectionDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection,
         kInvalidateSearchEngineChoiceOnRestoreVariations,
         "InvalidateSearchEngineChoiceOnDeviceRestoreDetection")},
    {"ios-provides-app-notification-settings",
     flag_descriptions::kIOSProvidesAppNotificationSettingsName,
     flag_descriptions::kIOSProvidesAppNotificationSettingsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSProvidesAppNotificationSettings)},
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
    {"enable-lens-context-menu-unified-experience",
     flag_descriptions::kEnableLensContextMenuUnifiedExperienceName,
     flag_descriptions::kEnableLensContextMenuUnifiedExperienceDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLensContextMenuUnifiedExperience)},
    {"enable-lens-in-omnibox-copied-image",
     flag_descriptions::kEnableLensInOmniboxCopiedImageName,
     flag_descriptions::kEnableLensInOmniboxCopiedImageDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableLensInOmniboxCopiedImage)},
    {"enable-lens-overlay", flag_descriptions::kEnableLensOverlayName,
     flag_descriptions::kEnableLensOverlayDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLensOverlay)},
    {"enable-lens-view-finder-unified-experience",
     flag_descriptions::kEnableLensViewFinderUnifiedExperienceName,
     flag_descriptions::kEnableLensViewFinderUnifiedExperienceDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLensViewFinderUnifiedExperience)},
    {"enable-disco-feed-endpoint",
     flag_descriptions::kEnableDiscoverFeedDiscoFeedEndpointName,
     flag_descriptions::kEnableDiscoverFeedDiscoFeedEndpointDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableDiscoverFeedDiscoFeedEndpoint)},
    {"shared-highlighting-amp",
     flag_descriptions::kIOSSharedHighlightingAmpName,
     flag_descriptions::kIOSSharedHighlightingAmpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingAmp)},
    {"track-by-default-mobile",
     commerce::flag_descriptions::kTrackByDefaultOnMobileName,
     commerce::flag_descriptions::kTrackByDefaultOnMobileDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(commerce::kTrackByDefaultOnMobile)},
    {"ntp-view-hierarchy-repair",
     flag_descriptions::kNTPViewHierarchyRepairName,
     flag_descriptions::kNTPViewHierarchyRepairDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableNTPViewHierarchyRepair)},
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
    {"intents-on-measurements", flag_descriptions::kMeasurementsName,
     flag_descriptions::kMeasurementsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableMeasurements)},
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
    {"enterprise-realtime-event-reporting-on-ios",
     flag_descriptions::kEnterpriseRealtimeEventReportingOnIOSName,
     flag_descriptions::kEnterpriseRealtimeEventReportingOnIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         enterprise_connectors::kEnterpriseRealtimeEventReportingOnIOS)},
    {"content-suggestions-magic-stack", flag_descriptions::kMagicStackName,
     flag_descriptions::kMagicStackDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMagicStack)},
    {"ios-keyboard-accessory-upgrade-for-ipad",
     flag_descriptions::kIOSKeyboardAccessoryUpgradeForIPadName,
     flag_descriptions::kIOSKeyboardAccessoryUpgradeForIPadDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSKeyboardAccessoryUpgradeForIPad)},
    {"ios-keyboard-accessory-upgrade-short-manual-fill-menu",
     flag_descriptions::kIOSKeyboardAccessoryUpgradeShortManualFillMenuName,
     flag_descriptions::
         kIOSKeyboardAccessoryUpgradeShortManualFillMenuDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSKeyboardAccessoryUpgradeShortManualFillMenu)},
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
    {"ios-password-bottom-sheet-autofocus",
     flag_descriptions::kIOSPasswordBottomSheetAutofocusName,
     flag_descriptions::kIOSPasswordBottomSheetAutofocusDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kIOSPasswordBottomSheetAutofocus)},
    {"ios-proactive-password-generation-bottom-sheet",
     flag_descriptions::kIOSProactivePasswordGenerationBottomSheetName,
     flag_descriptions::kIOSProactivePasswordGenerationBottomSheetDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kIOSProactivePasswordGenerationBottomSheet)},
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
    {"default-browser-banner-promo",
     flag_descriptions::kDefaultBrowserBannerPromoName,
     flag_descriptions::kDefaultBrowserBannerPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDefaultBrowserBannerPromo)},
    {"default-browser-promo-trigger-criteria-experiment",
     flag_descriptions::kDefaultBrowserTriggerCriteriaExperimentName,
     flag_descriptions::kDefaultBrowserTriggerCriteriaExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         feature_engagement::kDefaultBrowserTriggerCriteriaExperiment)},
    {"blue-dot-on-tools-menu-button",
     flag_descriptions::kBlueDotOnToolsMenuButtonName,
     flag_descriptions::kBlueDotOnToolsMenuButtonDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kBlueDotOnToolsMenuButton)},
#if BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"feed-background-refresh-ios",
     flag_descriptions::kFeedBackgroundRefreshName,
     flag_descriptions::kFeedBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFeedBackgroundRefresh,
                                    kFeedBackgroundRefreshVariations,
                                    "FeedBackgroundRefresh")},
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
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
    {"notification-settings-menu-item",
     flag_descriptions::kNotificationSettingsMenuItemName,
     flag_descriptions::kNotificationSettingsMenuItemDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNotificationSettingsMenuItem)},
    {"web-feed-feedback-reroute",
     flag_descriptions::kWebFeedFeedbackRerouteName,
     flag_descriptions::kWebFeedFeedbackRerouteDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kWebFeedFeedbackReroute)},
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
    {"only-access-clipboard-async",
     flag_descriptions::kOnlyAccessClipboardAsyncName,
     flag_descriptions::kOnlyAccessClipboardAsyncDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOnlyAccessClipboardAsync)},
    {"enable-signed-out-view-demotion",
     flag_descriptions::kEnableSignedOutViewDemotionName,
     flag_descriptions::kEnableSignedOutViewDemotionDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableSignedOutViewDemotion)},
    {"spotlight-never-retain-index",
     flag_descriptions::kSpotlightNeverRetainIndexName,
     flag_descriptions::kSpotlightNeverRetainIndexDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSpotlightNeverRetainIndex)},
    {"safety-check-magic-stack", flag_descriptions::kSafetyCheckMagicStackName,
     flag_descriptions::kSafetyCheckMagicStackDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSafetyCheckMagicStack)},
    {"tab-resumption", flag_descriptions::kTabResumptionName,
     flag_descriptions::kTabResumptionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabResumption)},
    {"bottom-omnibox-default-setting",
     flag_descriptions::kBottomOmniboxDefaultSettingName,
     flag_descriptions::kBottomOmniboxDefaultSettingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBottomOmniboxDefaultSetting,
                                    kBottomOmniboxDefaultSettingVariations,
                                    "BottomOmniboxDefaultSetting")},
    {"enable-save-to-photos-improvements",
     flag_descriptions::kIOSSaveToPhotosImprovementsName,
     flag_descriptions::kIOSSaveToPhotosImprovementsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSSaveToPhotosImprovements,
                                    kSaveToPhotosImprovementsVariations,
                                    "IOSSaveToPhotosImprovements")},
    {"enable-identity-in-auth-error",
     flag_descriptions::kEnableIdentityInAuthErrorName,
     flag_descriptions::kEnableIdentityInAuthErrorDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kEnableIdentityInAuthError)},
    {"enable-asweb-authentication-session",
     flag_descriptions::kEnableASWebAuthenticationSessionName,
     flag_descriptions::kEnableASWebAuthenticationSessionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kEnableASWebAuthenticationSession)},
    {"top-toolbar-theme-color", flag_descriptions::kThemeColorInTopToolbarName,
     flag_descriptions::kThemeColorInTopToolbarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kThemeColorInTopToolbar)},
    {"privacy-guide-ios", flag_descriptions::kPrivacyGuideIosName,
     flag_descriptions::kPrivacyGuideIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPrivacyGuideIos)},
    {"fullscreen-improvement", flag_descriptions::kFullscreenImprovementName,
     flag_descriptions::kFullscreenImprovementDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenImprovement)},
    {"autofill-enable-dynamically-loading-fields-on-input",
     flag_descriptions::
         kAutofillEnableDynamicallyLoadingFieldsForAddressInputName,
     flag_descriptions::
         kAutofillEnableDynamicallyLoadingFieldsForAddressInputDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillDynamicallyLoadsFieldsForAddressInput)},
    {"share-in-web-context-menu-ios",
     flag_descriptions::kShareInWebContextMenuIOSName,
     flag_descriptions::kShareInWebContextMenuIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kShareInWebContextMenuIOS)},
    {"https-upgrades-ios", flag_descriptions::kHttpsUpgradesName,
     flag_descriptions::kHttpsUpgradesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(security_interstitials::features::kHttpsUpgrades)},
    {"contextual-panel", flag_descriptions::kContextualPanelName,
     flag_descriptions::kContextualPanelDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kContextualPanel,
                                    kContextualPanelEntrypointArmVariations,
                                    "ContextualPanel")},
    {"enable-ipad-feed-ghost-cards",
     flag_descriptions::kEnableiPadFeedGhostCardsName,
     flag_descriptions::kEnableiPadFeedGhostCardsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableiPadFeedGhostCards)},
    {"disable-lens-camera", flag_descriptions::kDisableLensCameraName,
     flag_descriptions::kDisableLensCameraDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDisableLensCamera)},
    {"autofill-enable-card-benefits-for-american-express",
     flag_descriptions::kAutofillEnableCardBenefitsForAmericanExpressName,
     flag_descriptions::
         kAutofillEnableCardBenefitsForAmericanExpressDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardBenefitsForAmericanExpress)},
    {"autofill-enable-card-benefits-sync",
     flag_descriptions::kAutofillEnableCardBenefitsSyncName,
     flag_descriptions::kAutofillEnableCardBenefitsSyncDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardBenefitsSync)},
    {"linked-services-setting-ios",
     flag_descriptions::kLinkedServicesSettingIosName,
     flag_descriptions::kLinkedServicesSettingIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLinkedServicesSettingIos)},
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
    {"cpe-automatic-passkey-upgrade",
     flag_descriptions::kCredentialProviderAutomaticPasskeyUpgradeName,
     flag_descriptions::kCredentialProviderAutomaticPasskeyUpgradeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCredentialProviderAutomaticPasskeyUpgrade)},
    {"cpe-passkey-prf-support",
     flag_descriptions::kCredentialProviderPasskeyPRFName,
     flag_descriptions::kCredentialProviderPasskeyPRFDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCredentialProviderPasskeyPRF)},
    {"cpe-performance-improvements",
     flag_descriptions::kCredentialProviderPerformanceImprovementsName,
     flag_descriptions::kCredentialProviderPerformanceImprovementsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCredentialProviderPerformanceImprovements)},
    {"password-form-clientside-classifier",
     flag_descriptions::kPasswordFormClientsideClassifierName,
     flag_descriptions::kPasswordFormClientsideClassifierDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordFormClientsideClassifier)},
    {"identity-disc-account-menu",
     flag_descriptions::kIdentityDiscAccountMenuName,
     flag_descriptions::kIdentityDiscAccountMenuDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIdentityDiscAccountMenu,
                                    kIdentityDiscAccountMenuVariations,
                                    "IdentityDiscAccountMenu")},
    {"identity-confirmation-snackbar",
     flag_descriptions::kIdentityConfirmationSnackbarName,
     flag_descriptions::kIdentityConfirmationSnackbarDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kIdentityConfirmationSnackbar,
         kIdentityConfirmationSnackbarTestingVariations,
         "IdentityConfirmationSnackbar")},
    {"ios-quick-delete", flag_descriptions::kIOSQuickDeleteName,
     flag_descriptions::kIOSQuickDeleteDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSQuickDelete)},
    {"tab-group-sync", flag_descriptions::kTabGroupSyncName,
     flag_descriptions::kTabGroupSyncDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabGroupSync)},
    {"omnibox-suggestion-answer-migration",
     flag_descriptions::kOmniboxSuggestionAnswerMigrationName,
     flag_descriptions::kOmniboxSuggestionAnswerMigrationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox_feature_configs::SuggestionAnswerMigration::
                            kOmniboxSuggestionAnswerMigration)},
    {"send-tab-ios-push-notifications",
     flag_descriptions::kSendTabToSelfIOSPushNotificationsName,
     flag_descriptions::kSendTabToSelfIOSPushNotificationsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         send_tab_to_self::kSendTabToSelfIOSPushNotifications,
         kSendTabIOSPushNotificationsVariations,
         "SendTabToSelfIOSPushNotifications")},
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
    {"ios-choose-from-drive", flag_descriptions::kIOSChooseFromDriveName,
     flag_descriptions::kIOSChooseFromDriveDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSChooseFromDrive)},
    {"omnibox-mia-zps", flag_descriptions::kOmniboxMiaZps,
     flag_descriptions::kOmniboxMiaZpsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox_feature_configs::MiaZPS::kOmniboxMiaZPS,
         kOmniboxMiaZpsVariations,
         "OmniboxMiaZpsVariations")},
    {"omnibox-ml-log-url-scoring-signals",
     flag_descriptions::kOmniboxMlLogUrlScoringSignalsName,
     flag_descriptions::kOmniboxMlLogUrlScoringSignalsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kLogUrlScoringSignals)},
    {"omnibox-ml-url-piecewise-mapped-search-blending",
     flag_descriptions::kOmniboxMlUrlPiecewiseMappedSearchBlendingName,
     flag_descriptions::kOmniboxMlUrlPiecewiseMappedSearchBlendingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kMlUrlPiecewiseMappedSearchBlending,
         kMlUrlPiecewiseMappedSearchBlendingVariations,
         "MlUrlPiecewiseMappedSearchBlending")},
    {"omnibox-ml-url-score-caching",
     flag_descriptions::kOmniboxMlUrlScoreCachingName,
     flag_descriptions::kOmniboxMlUrlScoreCachingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kMlUrlScoreCaching)},
    {"omnibox-ml-url-scoring", flag_descriptions::kOmniboxMlUrlScoringName,
     flag_descriptions::kOmniboxMlUrlScoringDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kMlUrlScoring,
                                    kOmniboxMlUrlScoringVariations,
                                    "MlUrlScoring")},
    {"omnibox-ml-url-search-blending",
     flag_descriptions::kOmniboxMlUrlSearchBlendingName,
     flag_descriptions::kOmniboxMlUrlSearchBlendingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kMlUrlSearchBlending,
                                    kMlUrlSearchBlendingVariations,
                                    "MlUrlScoring")},
    {"omnibox-ml-url-scoring-model",
     flag_descriptions::kOmniboxMlUrlScoringModelName,
     flag_descriptions::kOmniboxMlUrlScoringModelDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kUrlScoringModel,
                                    kUrlScoringModelVariations,
                                    "MlUrlScoring")},
    {"autofill-show-manual-fill-for-virtual-cards",
     flag_descriptions::kAutofillShowManualFillForVirtualCardsName,
     flag_descriptions::kAutofillShowManualFillForVirtualCardsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillShowManualFillForVirtualCards)},
    {"safety-check-notifications",
     flag_descriptions::kSafetyCheckNotificationsName,
     flag_descriptions::kSafetyCheckNotificationsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kSafetyCheckNotifications,
                                    kSafetyCheckNotificationsVariations,
                                    "SafetyCheckNotifications")},
    {"app-background-refresh-ios", flag_descriptions::kAppBackgroundRefreshName,
     flag_descriptions::kAppBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableAppBackgroundRefresh)},
    {"home-memory-improvements", flag_descriptions::kHomeMemoryImprovementsName,
     flag_descriptions::kHomeMemoryImprovementsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kHomeMemoryImprovements)},
    {"autofill-upload-card-request-timeout",
     flag_descriptions::kAutofillUploadCardRequestTimeoutName,
     flag_descriptions::kAutofillUploadCardRequestTimeoutDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUploadCardRequestTimeout,
         kAutofillUploadCardRequestTimeoutOptions,
         "AutofillUploadCardRequestTimeout")},
    {"autofill-vcn-enroll-request-timeout",
     flag_descriptions::kAutofillVcnEnrollRequestTimeoutName,
     flag_descriptions::kAutofillVcnEnrollRequestTimeoutDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillVcnEnrollRequestTimeout,
         kAutofillVcnEnrollRequestTimeoutOptions,
         "AutofillVcnEnrollRequestTimeout")},
    {"lens-web-page-load-optimization-enabled",
     flag_descriptions::kLensWebPageLoadOptimizationEnabledName,
     flag_descriptions::kLensWebPageLoadOptimizationEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensWebPageLoadOptimizationEnabled)},
    {"segmented-default-browser-promo",
     flag_descriptions::kSegmentedDefaultBrowserPromoName,
     flag_descriptions::kSegmentedDefaultBrowserPromoDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kSegmentedDefaultBrowserPromo,
                                    kSegmentedDefaultBrowserPromoVariations,
                                    "SegmentedDBP-Animation")},
    {"autofill-unmask-card-request-timeout",
     flag_descriptions::kAutofillUnmaskCardRequestTimeoutName,
     flag_descriptions::kAutofillUnmaskCardRequestTimeoutDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUnmaskCardRequestTimeout)},
    {"autofill-across-iframes", flag_descriptions::kAutofillAcrossIframesName,
     flag_descriptions::kAutofillAcrossIframesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAcrossIframesIos)},
    {"ios-page-info-last-visited",
     flag_descriptions::kPageInfoLastVisitedIOSName,
     flag_descriptions::kPageInfoLastVisitedIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPageInfoLastVisitedIOS)},
    {"enable-trait-collection-registration",
     flag_descriptions::kEnableTraitCollectionRegistrationName,
     flag_descriptions::kEnableTraitCollectionRegistrationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableTraitCollectionRegistration)},
    {"autofill-isolated-world-ios",
     flag_descriptions::kAutofillIsolatedWorldForJavascriptIOSName,
     flag_descriptions::kAutofillIsolatedWorldForJavascriptIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillIsolatedWorldForJavascriptIos)},
    {"tab-group-indicator", flag_descriptions::kTabGroupIndicatorName,
     flag_descriptions::kTabGroupIndicatorDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kTabGroupIndicator,
                                    kTabGroupIndicatorVariations,
                                    "TabGroupIndicator")},
    {"safe-browsing-local-lists-use-sbv5",
     flag_descriptions::kSafeBrowsingLocalListsUseSBv5Name,
     flag_descriptions::kSafeBrowsingLocalListsUseSBv5Description,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(safe_browsing::kLocalListsUseSBv5)},
    {"ios-price-tracking-notification-promo-card",
     flag_descriptions::kPriceTrackingPromoName,
     flag_descriptions::kPriceTrackingPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kPriceTrackingPromo)},
    {"ios-shop-card", flag_descriptions::kShopCardName,
     flag_descriptions::kShopCardDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kShopCard,
                                    kShopCardOverrideOptions,
                                    "ShopCard")},
    {"ios-shop-card-impression-limits",
     flag_descriptions::kShopCardImpressionLimitsName,
     flag_descriptions::kShopCardImpressionLimitsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kShopCardImpressionLimits)},
    {"ios-segmentation-ephemeral-card-ranker",
     flag_descriptions::kSegmentationPlatformEphemeralCardRankerName,
     flag_descriptions::kSegmentationPlatformEphemeralCardRankerDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         segmentation_platform::features::
             kSegmentationPlatformEphemeralCardRanker,
         kEphemeralCardRankerCardOverrideOptions,
         "SegmentationPlatformEphemeralCardRanker")},
    {"autofill-enable-log-form-events-to-all-parsed-form-types",
     flag_descriptions::kAutofillEnableLogFormEventsToAllParsedFormTypesName,
     flag_descriptions::
         kAutofillEnableLogFormEventsToAllParsedFormTypesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableLogFormEventsToAllParsedFormTypes)},
    {"lens-overlay-enable-ipad-compatibility",
     flag_descriptions::kLensOverlayEnableIPadCompatibilityName,
     flag_descriptions::kLensOverlayEnableIPadCompatibilityDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayEnableIPadCompatibility)},
    {"lens-overlay-enable-landscape-compatibility",
     flag_descriptions::kLensOverlayEnableLandscapeCompatibilityName,
     flag_descriptions::kLensOverlayEnableLandscapeCompatibilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensOverlayEnableLandscapeCompatibility)},
    {"lens-overlay-force-show-onboarding-screen",
     flag_descriptions::kLensOverlayForceShowOnboardingScreenName,
     flag_descriptions::kLensOverlayForceShowOnboardingScreenDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensOverlayForceShowOnboardingScreen)},
    {"lens-overlay-alternative-onboarding",
     flag_descriptions::kLensOverlayAlternativeOnboardingName,
     flag_descriptions::kLensOverlayAlternativeOnboardingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kLensOverlayAlternativeOnboarding,
                                    kLensOverlayOnboardingVariations,
                                    "kLensOverlayOnboarding")},
    {"data-sharing", flag_descriptions::kDataSharingName,
     flag_descriptions::kDataSharingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(data_sharing::features::kDataSharingFeature)},
    {"data-sharing-join-only", flag_descriptions::kDataSharingJoinOnlyName,
     flag_descriptions::kDataSharingJoinOnlyDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(data_sharing::features::kDataSharingJoinOnly)},
    {"ios-soft-lock", flag_descriptions::kIOSSoftLockName,
     flag_descriptions::kIOSSoftLockDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSSoftLock,
                                    kIOSSoftLockVariations,
                                    "IOSSoftLock")},
    {"separate-profiles-for-managed-accounts",
     flag_descriptions::kSeparateProfilesForManagedAccountsName,
     flag_descriptions::kSeparateProfilesForManagedAccountsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kSeparateProfilesForManagedAccounts)},
    {"tab-resumption-images", flag_descriptions::kTabResumptionImagesName,
     flag_descriptions::kTabResumptionImagesDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kTabResumptionImages,
                                    kTabResumptionImagesVariations,
                                    "TabResumption1_5")},
    {"segmentation-platform-tips-ephemeral-card",
     flag_descriptions::kSegmentationPlatformTipsEphemeralCardName,
     flag_descriptions::kSegmentationPlatformTipsEphemeralCardDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(segmentation_platform::features::
                            kSegmentationPlatformTipsEphemeralCard)},
    {"ios-password-suggestion-bottom-sheet-v2",
     flag_descriptions::kPasswordSuggestionBottomSheetV2Name,
     flag_descriptions::kPasswordSuggestionBottomSheetV2Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kIOSPasswordBottomSheetV2)},
    {"lens-unary-apis-with-http-transport-enabled",
     flag_descriptions::kLensUnaryApisWithHttpTransportEnabledName,
     flag_descriptions::kLensUnaryApisWithHttpTransportEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensUnaryApisWithHttpTransportEnabled)},
    {"lens-overlay-enable-same-tab-navigation",
     flag_descriptions::kLensOverlayEnableSameTabNavigationName,
     flag_descriptions::kLensOverlayEnableSameTabNavigationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayEnableSameTabNavigation)},
    {"ios-chrome-startup-parameters-async",
     flag_descriptions::kChromeStartupParametersAsyncName,
     flag_descriptions::kChromeStartupParametersAsyncDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kChromeStartupParametersAsync)},
    {"ios-youtube-incognito", flag_descriptions::kYoutubeIncognitoName,
     flag_descriptions::kYoutubeIncognitoDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kYoutubeIncognito,
                                    kYoutubeIncognitoVariations,
                                    "IOSYoutubeIncognito")},
    {"lens-overlay-enable-location-bar-entrypoint",
     flag_descriptions::kLensOverlayEnableLocationBarEntrypointName,
     flag_descriptions::kLensOverlayEnableLocationBarEntrypointDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensOverlayEnableLocationBarEntrypoint)},
    {"lens-overlay-enable-location-bar-entrypoint-on-srp",
     flag_descriptions::kLensOverlayEnableLocationBarEntrypointOnSRPName,
     flag_descriptions::kLensOverlayEnableLocationBarEntrypointOnSRPDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensOverlayEnableLocationBarEntrypointOnSRP)},
    {"lens-overlay-enable-lvf-escape-hatch",
     flag_descriptions::kLensOverlayEnableLVFEscapeHatchName,
     flag_descriptions::kLensOverlayEnableLVFEscapeHatchDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayEnableLVFEscapeHatch)},
    {"lens-overlay-disable-iph-pan-gesture",
     flag_descriptions::kLensOverlayDisableIPHPanGestureName,
     flag_descriptions::kLensOverlayDisableIPHPanGestureDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayDisableIPHPanGesture)},
    {"lens-overlay-disable-price-insights",
     flag_descriptions::kLensOverlayDisablePriceInsightsName,
     flag_descriptions::kLensOverlayDisablePriceInsightsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayDisablePriceInsights)},
    {"ios-provisional-notification-alert",
     flag_descriptions::kProvisionalNotificationAlertName,
     flag_descriptions::kProvisionalNotificationAlertDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kProvisionalNotificationAlert)},
    {"ios-start-time-startup-remediations",
     flag_descriptions::kIOSStartTimeStartupRemediationsName,
     flag_descriptions::kIOSStartTimeStartupRemediationsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSStartTimeStartupRemediations,
                                    kIOSStartTimeStartupRemediationsVariations,
                                    "IOSStartTimeStartupRemediations")},
    {"autofill-throttle-doc-form-scans",
     flag_descriptions::kAutofillThrottleDocumentFormScanName,
     flag_descriptions::kAutofillThrottleDocumentFormScanDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kAutofillThrottleDocumentFormScanIos,
                                    kAutofillThrottleDocFormScanVariations,
                                    "AutofillThrottleDocumentFormScan")},
    {"autofill-throttle-filtered-doc-form-scan",
     flag_descriptions::kAutofillThrottleFilteredDocumentFormScanName,
     flag_descriptions::kAutofillThrottleFilteredDocumentFormScanDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kAutofillThrottleFilteredDocumentFormScanIos,
         kAutofillThrottleFilteredDocFormScanVariations,
         "AutofillThrottleFilteredDocumentFormScan")},
    {"autofill-payments-sheet-v2",
     flag_descriptions::kAutofillPaymentsSheetV2Name,
     flag_descriptions::kAutofillPaymentsSheetV2Description, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillPaymentsSheetV2Ios)},
    {"ios-start-time-browser-background-remediations",
     flag_descriptions::kIOSStartTimeBrowserBackgroundRemediationsName,
     flag_descriptions::kIOSStartTimeBrowserBackgroundRemediationsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kIOSStartTimeBrowserBackgroundRemediations,
         kIOSStartTimeBrowserBackgroundRemediationsVariations,
         "IOSStartTimeStartupRemediations")},
    {"lens-unary-http-transport-enabled",
     flag_descriptions::kLensUnaryHttpTransportEnabledName,
     flag_descriptions::kLensUnaryHttpTransportEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensUnaryHttpTransportEnabled)},
    {"lens-clearcut-background-upload-enabled",
     flag_descriptions::kLensClearcutBackgroundUploadEnabledName,
     flag_descriptions::kLensClearcutBackgroundUploadEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensClearcutBackgroundUploadEnabled)},
    {"lens-clearcut-logger-fast-qos-enabled",
     flag_descriptions::kLensClearcutLoggerFastQosEnabledName,
     flag_descriptions::kLensClearcutLoggerFastQosEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensClearcutLoggerFastQosEnabled)},
    {"set-up-list-shortened-duration",
     flag_descriptions::kSetUpListShortenedDurationName,
     flag_descriptions::kSetUpListShortenedDurationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(set_up_list::kSetUpListShortenedDuration,
                                    kSetUpListDurationVariations,
                                    "SetUpListShortenedDuration")},
    {"enable-lens-overlay-price-insights-counterfactual",
     flag_descriptions::kLensOverlayPriceInsightsCounterfactualName,
     flag_descriptions::kLensOverlayPriceInsightsCounterfactualDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensOverlayPriceInsightsCounterfactual)},
    {"collaboration-messaging", flag_descriptions::kCollaborationMessagingName,
     flag_descriptions::kCollaborationMessagingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(collaboration::features::kCollaborationMessaging)},
    {"lens-single-tap-text-selection-disabled",
     flag_descriptions::kLensSingleTapTextSelectionDisabledName,
     flag_descriptions::kLensSingleTapTextSelectionDisabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensSingleTapTextSelectionDisabled)},
    {"updated-fre-screens-sequence", flag_descriptions::kUpdatedFRESequenceName,
     flag_descriptions::kUpdatedFRESequenceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(first_run::kUpdatedFirstRunSequence,
                                    kUpdatedFirstRunSequenceVariations,
                                    "UpdatedFirstRunSequence")},
    {"set-up-list-without-sign-in-item",
     flag_descriptions::kSetUpListWithoutSignInItemName,
     flag_descriptions::kSetUpListWithoutSignInItemDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(set_up_list::kSetUpListWithoutSignInItem)},
    {"autofill-enable-card-benefits-for-bmo",
     flag_descriptions::kAutofillEnableCardBenefitsForBmoName,
     flag_descriptions::kAutofillEnableCardBenefitsForBmoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableCardBenefitsForBmo)},
    {"ios-manage-account-storage",
     flag_descriptions::kIOSManageAccountStorageName,
     flag_descriptions::kIOSManageAccountStorageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSManageAccountStorage)},
    {"supervised-user-local-web-approvals",
     flag_descriptions::kSupervisedUserLocalWebApprovalsName,
     flag_descriptions::kSupervisedUserLocalWebApprovalsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(supervised_user::kLocalWebApprovals)},
    {"download-auto-deletion", flag_descriptions::kDownloadAutoDeletionName,
     flag_descriptions::kDownloadAutoDeletionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDownloadAutoDeletionFeatureEnabled)},
    {"lens-ink-multi-sample-mode-disabled",
     flag_descriptions::kLensInkMultiSampleModeDisabledName,
     flag_descriptions::kLensInkMultiSampleModeDisabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensInkMultiSampleModeDisabled)},
    {"animated-default-browser-promo-in-fre",
     flag_descriptions::kAnimatedDefaultBrowserPromoInFREName,
     flag_descriptions::kAnimatedDefaultBrowserPromoInFREDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         first_run::kAnimatedDefaultBrowserPromoInFRE,
         kAnimatedDefaultBrowserPromoInFREVariations,
         "AnimatedDBPInFRE-Layout")},
    {"autofill-enable-allowlist-for-bmo-card-category-benefits",
     flag_descriptions::kAutofillEnableAllowlistForBmoCardCategoryBenefitsName,
     flag_descriptions::
         kAutofillEnableAllowlistForBmoCardCategoryBenefitsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableAllowlistForBmoCardCategoryBenefits)},
    {"fullscreen-transition", flag_descriptions::kFullscreenTransitionName,
     flag_descriptions::kFullscreenTransitionDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kFullscreenTransition,
                                    kFullscreenTransitionVariations,
                                    "IOSFull`screenTransition")},
    {"ios-deprecate-feed-header",
     flag_descriptions::kDeprecateFeedHeaderExperimentName,
     flag_descriptions::kDeprecateFeedHeaderExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDeprecateFeedHeader,
                                    kDeprecateFeedHeaderVariations,
                                    "IOSDeprecateFeedHeader")},
    {"refactor-toolbars-size", flag_descriptions::kRefactorToolbarsSizeName,
     flag_descriptions::kRefactorToolbarsSizeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kRefactorToolbarsSize)},
    {"lens-gesture-text-selection-disabled",
     flag_descriptions::kLensGestureTextSelectionDisabledName,
     flag_descriptions::kLensGestureTextSelectionDisabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensGestureTextSelectionDisabled)},
    {"add-address-manually", flag_descriptions::kAddAddressManuallyName,
     flag_descriptions::kAddAddressManuallyDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAddAddressManually)},
    {"lens-vsint-param-enabled", flag_descriptions::kLensVsintParamEnabledName,
     flag_descriptions::kLensVsintParamEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensVsintParamEnabled)},
    {"lens-unary-client-data-header-enabled",
     flag_descriptions::kLensUnaryClientDataHeaderEnabledName,
     flag_descriptions::kLensUnaryClientDataHeaderEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensUnaryClientDataHeaderEnabled)},
    {"ios-new-share-extension", flag_descriptions::kNewShareExtensionName,
     flag_descriptions::kNewShareExtensionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewShareExtension)},
    {"ios-best-features-screen",
     flag_descriptions::kBestFeaturesScreenInFirstRunName,
     flag_descriptions::kBestFeaturesScreenInFirstRunDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(first_run::kBestFeaturesScreenInFirstRun,
                                    kBestFeaturesScreenInFirstRunVariations,
                                    "BestFeaturesScreenInFirstRun")},
    {"ios-passkeys-m2", flag_descriptions::kIOSPasskeysM2Name,
     flag_descriptions::kIOSPasskeysM2Description, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSPasskeysM2)},
    {"manual-log-uploads-in-the-fre",
     flag_descriptions::kManualLogUploadsInFREName,
     flag_descriptions::kManualLogUploadsInFREDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(first_run::kManualLogUploadsInTheFRE)},
    {"autofill-disable-default-save-card-fix-flow-detection",
     flag_descriptions::kAutofillDisableDefaultSaveCardFixFlowDetectionName,
     flag_descriptions::
         kAutofillDisableDefaultSaveCardFixFlowDetectionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillDisableDefaultSaveCardFixFlowDetection)},
    {"lens-unary-api-salient-text-enabled",
     flag_descriptions::kLensUnaryApiSalientTextEnabledName,
     flag_descriptions::kLensUnaryApiSalientTextEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensUnaryApiSalientTextEnabled)},
    {"non-modal-sign-in-promo", flag_descriptions::kNonModalSignInPromoName,
     flag_descriptions::kNonModalSignInPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNonModalSignInPromo)},
    {"suggest-strong-password-in-add-password",
     flag_descriptions::kSuggestStrongPasswordInAddPasswordName,
     flag_descriptions::kSuggestStrongPasswordInAddPasswordDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kSuggestStrongPasswordInAddPassword)},
    {"autofill-save-card-bottomsheet",
     flag_descriptions::kAutofillSaveCardBottomSheetName,
     flag_descriptions::kAutofillSaveCardBottomSheetDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillSaveCardBottomSheet)},
    {"ios-one-tap-mini-map-restrictions",
     flag_descriptions::kIOSOneTapMiniMapRestrictionsName,
     flag_descriptions::kIOSOneTapMiniMapRestrictionsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSOneTapMiniMapRestrictions,
                                    kIOSOneTapMiniMapRestrictionsVariations,
                                    "IOSOneTapMiniMapRestrictions")},
    {"lens-block-fetch-objects-interaction-rpcs-on-separate-handshake",
     flag_descriptions::
         kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeName,
     flag_descriptions::
         kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshakeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         kLensBlockFetchObjectsInteractionRPCsOnSeparateHandshake)},
    {"lens-prewarm-hard-stickiness-in-input-selection",
     flag_descriptions::kLensPrewarmHardStickinessInInputSelectionName,
     flag_descriptions::kLensPrewarmHardStickinessInInputSelectionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensPrewarmHardStickinessInInputSelection)},
    {"lens-prewarm-hard-stickiness-in-query-formulation",
     flag_descriptions::kLensPrewarmHardStickinessInQueryFormulationName,
     flag_descriptions::kLensPrewarmHardStickinessInQueryFormulationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensPrewarmHardStickinessInQueryFormulation)},
    {"enhanced-calendar", flag_descriptions::kEnhancedCalendarName,
     flag_descriptions::kEnhancedCalendarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnhancedCalendar)},
    {"data-sharing-debug-logs", flag_descriptions::kDataSharingDebugLogsName,
     flag_descriptions::kDataSharingDebugLogsDescription, flags_ui::kOsIos,
     SINGLE_VALUE_TYPE(data_sharing::kDataSharingDebugLoggingEnabled)},
    {"supervised-user-block-interstitial-v3",
     flag_descriptions::kSupervisedUserBlockInterstitialV3Name,
     flag_descriptions::kSupervisedUserBlockInterstitialV3Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(supervised_user::kSupervisedUserBlockInterstitialV3)},
    {"lens-fetch-srp-api-enabled",
     flag_descriptions::kLensFetchSrpApiEnabledName,
     flag_descriptions::kLensFetchSrpApiEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensFetchSrpApiEnabled)},
    {"autofill-enable-card-info-runtime-retrieval",
     flag_descriptions::kAutofillEnableCardInfoRuntimeRetrievalName,
     flag_descriptions::kAutofillEnableCardInfoRuntimeRetrievalDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableCardInfoRuntimeRetrieval)},
    {"feed-swipe-iph", flag_descriptions::kFeedSwipeInProductHelpName,
     flag_descriptions::kFeedSwipeInProductHelpDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kFeedSwipeInProductHelp,
                                    kFeedSwipeInProductHelpVariations,
                                    "FeedSwipeInProductHelp")},
    {"lens-qr-code-parsing-fix", flag_descriptions::kLensQRCodeParsingFixName,
     flag_descriptions::kLensQRCodeParsingFixDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensQRCodeParsingFix)},
    {"notification-collision-management",
     flag_descriptions::kNotificationCollisionManagementName,
     flag_descriptions::kNotificationCollisionManagementDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNotificationCollisionManagement)},
    {"ios-one-tap-mini-map-remove-section-breaks",
     flag_descriptions::kIOSOneTapMiniMapRemoveSectionBreaksName,
     flag_descriptions::kIOSOneTapMiniMapRemoveSectionBreaksDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSOneTapMiniMapRemoveSectionsBreaks)},
    {"autofill-enable-support-for-home-and-work",
     flag_descriptions::kAutofillEnableSupportForHomeAndWorkName,
     flag_descriptions::kAutofillEnableSupportForHomeAndWorkDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSupportForHomeAndWork)},
    {"lens-overlay-navigation-history",
     flag_descriptions::kLensOverlayNavigationHistoryName,
     flag_descriptions::kLensOverlayNavigationHistoryDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayNavigationHistory)},
    {"page-action-menu", flag_descriptions::kPageActionMenuName,
     flag_descriptions::kPageActionMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPageActionMenu)},
    {"glic-promo-consent", flag_descriptions::kGLICPromoConsentName,
     flag_descriptions::kGLICPromoConsentDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kGLICPromoConsent,
                                    kGLICPromoConsentVariations,
                                    "IOSGLICPromoConsent")},
    {"feedback-include-variations",
     flag_descriptions::kFeedbackIncludeVariationsName,
     flag_descriptions::kFeedbackIncludeVariationsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(variations::kFeedbackIncludeVariations)},
    {"safe-browsing-trusted-url",
     flag_descriptions::kSafeBrowsingTrustedURLName,
     flag_descriptions::kSafeBrowsingTrustedURLDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSafeBrowsingTrustedURL)},
    {"contained-tab-group", flag_descriptions::kContainedTabGroupName,
     flag_descriptions::kContainedTabGroupDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kContainedTabGroup)},
    {"sync-trusted-vault-infobar-improvements",
     flag_descriptions::kSyncTrustedVaultInfobarImprovementsName,
     flag_descriptions::kSyncTrustedVaultInfobarImprovementsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncTrustedVaultInfobarImprovements)},
    {"sync-trusted-vault-infobar-message-improvements",
     flag_descriptions::kSyncTrustedVaultInfobarMessageImprovementsName,
     flag_descriptions::kSyncTrustedVaultInfobarMessageImprovementsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncTrustedVaultInfobarMessageImprovements)},
    {"ios-choose-from-drive-simulated-click",
     flag_descriptions::kIOSChooseFromDriveSimulatedClickName,
     flag_descriptions::kIOSChooseFromDriveSimulatedClickDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSChooseFromDriveSimulatedClick)},
    {"autofill-vcn-enroll-strike-expiry-time",
     flag_descriptions::kAutofillVcnEnrollStrikeExpiryTimeName,
     flag_descriptions::kAutofillVcnEnrollStrikeExpiryTimeDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillVcnEnrollStrikeExpiryTime,
         kAutofillVcnEnrollStrikeExpiryTimeOptions,
         "AutofillVcnEnrollStrikeExpiryTime")},
    {"enable-enterprise-url-filtering",
     flag_descriptions::kIOSEnterpriseRealtimeUrlFilteringName,
     flag_descriptions::kIOSEnterpriseRealtimeUrlFilteringDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         enterprise_connectors::kIOSEnterpriseRealtimeUrlFiltering)},
    {"ios-welcome-back-screen", flag_descriptions::kWelcomeBackInFirstRunName,
     flag_descriptions::kWelcomeBackInFirstRunDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(first_run::kWelcomeBackInFirstRun,
                                    kWelcomeBackInFirstRunVariations,
                                    "WelcomeBackInFirstRun")},
    {"autofill-enable-flat-rate-card-benefits-from-curinos",
     flag_descriptions::kAutofillEnableFlatRateCardBenefitsFromCurinosName,
     flag_descriptions::
         kAutofillEnableFlatRateCardBenefitsFromCurinosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableFlatRateCardBenefitsFromCurinos)},
    {"reader-mode-enabled", flag_descriptions::kReaderModeName,
     flag_descriptions::kReaderModeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderMode)},
    {"reader-mode-debug-info-enabled",
     flag_descriptions::kReaderModeDebugInfoName,
     flag_descriptions::kReaderModeDebugInfoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderModeDebugInfo)},
    {"reader-mode-page-eligibility-enabled",
     flag_descriptions::kReaderModePageEligibilityHeuristicName,
     flag_descriptions::kReaderModePageEligibilityHeuristicDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableReaderModePageEligibilityForToolsMenu)},
    {"best-of-app-fre", flag_descriptions::kBestOfAppFREName,
     flag_descriptions::kBestOfAppFREDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBestOfAppFRE,
                                    kBestOfAppFREVariations,
                                    "BestOfAppFRE")},
    {"use-new-feed-eligibility-service",
     flag_descriptions::kUseFeedEligibilityServiceName,
     flag_descriptions::kUseFeedEligibilityServiceDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseFeedEligibilityService)},
    {"import-passwords-from-safari",
     flag_descriptions::kImportPasswordsFromSafariName,
     flag_descriptions::kImportPasswordsFromSafariDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kImportPasswordsFromSafari)},
    {"widgets-for-multiprofile", flag_descriptions::kWidgetsForMultiprofileName,
     flag_descriptions::kWidgetsForMultiprofileDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kWidgetsForMultiprofile)},
    {"enable-password-manager-trusted-vault-widget",
     flag_descriptions::kIOSEnablePasswordManagerTrustedVaultWidgetName,
     flag_descriptions::kIOSEnablePasswordManagerTrustedVaultWidgetDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kIOSEnablePasswordManagerTrustedVaultWidget)},
    {"colorful-tab-group", flag_descriptions::kColorfulTabGroupName,
     flag_descriptions::kColorfulTabGroupDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kColorfulTabGroup)},
    {"lens-load-aim-in-lens-result-page",
     flag_descriptions::kLensLoadAIMInLensResultPageName,
     flag_descriptions::kLensLoadAIMInLensResultPageDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensLoadAIMInLensResultPage)},
    {"lens-exact-matches-enabled",
     flag_descriptions::kLensExactMatchesEnabledName,
     flag_descriptions::kLensExactMatchesEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensExactMatchesEnabled)},
    {"autofill-local-save-card-bottomsheet",
     flag_descriptions::kAutofillLocalSaveCardBottomSheetName,
     flag_descriptions::kAutofillLocalSaveCardBottomSheetDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillLocalSaveCardBottomSheet)},
    {"autofill-enable-fpan-risk-based-authentication",
     flag_descriptions::kAutofillEnableFpanRiskBasedAuthenticationName,
     flag_descriptions::kAutofillEnableFpanRiskBasedAuthenticationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableFpanRiskBasedAuthentication)},
    {"autofill-enable-multiple-request-in-virtual-card-downstream-enrollment",
     flag_descriptions::
         kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentName,
     flag_descriptions::
         kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollmentDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)},
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
