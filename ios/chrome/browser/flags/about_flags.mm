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
#import "components/autofill/core/common/autofill_debug_features.h"
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
#import "components/contextual_tasks/public/features.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/switches.h"
#import "components/desktop_to_mobile_promos/features.h"
#import "components/dom_distiller/core/dom_distiller_switches.h"
#import "components/download/public/background_service/features.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/enterprise/browser/reporting/reporting_features.h"
#import "components/enterprise/client_certificates/core/features.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/enterprise/data_controls/core/browser/features.h"
#import "components/enterprise/net/core/features.h"
#import "components/enterprise/net/core/flag_descriptions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feed/feed_feature_list.h"
#import "components/history/core/browser/features.h"
#import "components/lens/lens_features.h"
#import "components/metrics/metrics_features.h"
#import "components/ntp_tiles/switches.h"
#import "components/omnibox/browser/aim_eligibility_service_features.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/common/composebox_features.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_logger.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/features.h"
#import "components/payments/core/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/ios/browser/web_ui/features.h"
#import "components/search/ntp_features.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/send_tab_to_self/features.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "components/sharing_message/features.h"
#import "components/signin/core/browser/account_reconcilor.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strike_database/strike_database_features.h"
#import "components/strings/grit/components_strings.h"
#import "components/supervised_user/core/common/features.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/features.h"
#import "components/sync/base/pref_names.h"
#import "components/sync_preferences/features.h"
#import "components/tab_groups/features.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/translate/core/common/translate_util.h"
#import "components/universal_optout/features.h"
#import "components/variations/net/variations_command_line.h"
#import "components/variations/variations_switches.h"
#import "components/wallet/core/common/wallet_features.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_features.h"
#import "components/webauthn/ios/features.h"
#import "components/webui/flags/feature_entry.h"
#import "components/webui/flags/feature_entry_macros.h"
#import "components/webui/flags/flags_storage.h"
#import "components/webui/flags/flags_ui_switches.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/app/background_task/features.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/badges/model/features.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/crash_report/model/features.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/browser/download/ui/features.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/features.h"
#import "ios/chrome/browser/enterprise/data_protection/public/features.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/flags/chrome_switches.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/features.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_classification/features.h"
#import "ios/chrome/browser/lens/ui_bundled/features.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/public/features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/text_selection/model/text_selection_util.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_fetcher.h"
#import "ios/chrome/browser/web/model/features.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/whats_new/coordinator/whats_new_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"
#import "ios/web/common/features.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/common/web_view_creation_util.h"

using flags_ui::FeatureEntry;

namespace {

constexpr FeatureEntry::Choice kSendTabToSelfEnhancedHandoffChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"Enabled with fast-follows", switches::kEnableFeatures,
     "IPH_SendTabToSelfOmnibox,"
     "SendTabToSelfSupportAutoOpenInTabGrid,"
     "SyncDisambiguateDeviceNamesWithChannel,"
     "SendTabToSelfIOSShareSheetDeviceList,"
     "SendTabToSelfMultiTabShare"},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kDisableFeatures,
     "IPH_SendTabToSelfOmnibox,"
     "SendTabToSelfSupportAutoOpenInTabGrid,"
     "SyncDisambiguateDeviceNamesWithChannel,"
     "SendTabToSelfIOSShareSheetDeviceList,"
     "SendTabToSelfMultiTabShare"},
};

constexpr FeatureEntry::Choice
    kWaitThresholdMillisecondsForCapabilitiesApiChoices[] = {
        {flags_ui::kGenericExperimentChoiceDefault, "", ""},
        {"200", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "200"},
        {"500", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "500"},
        {"5000", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "5000"},
};

constexpr FeatureEntry::FeatureParam kActorToolsPageStabilityEnabled[] = {
    {kActorToolsPageStabilityParam, "true"},
};
constexpr FeatureEntry::FeatureVariation kActorToolsPageStabilityVariations[] =
    {
        {"PageStabilityEnabled", kActorToolsPageStabilityEnabled, nullptr},
};

constexpr FeatureEntry::FeatureParam
    kAutofillAiWalletPassBranding2026Variant1[] = {{"string_variant", "1"}};
constexpr FeatureEntry::FeatureParam
    kAutofillAiWalletPassBranding2026Variant2[] = {{"string_variant", "2"}};

constexpr FeatureEntry::FeatureVariation
    kAutofillAiWalletPassBranding2026Variations[] = {
        {"Variant 1 (Securely + Wallet)",
         kAutofillAiWalletPassBranding2026Variant1, nullptr},
        {"Variant 2 (Securely only)", kAutofillAiWalletPassBranding2026Variant2,
         nullptr},
};

constexpr FeatureEntry::FeatureParam
    kDisableKeyboardAccessoryOnlySymbolsParam[] = {
        {kDisableKeyboardAccessoryParam, kDisableKeyboardAccessoryOnlySymbols}};

constexpr FeatureEntry::FeatureParam
    kDisableKeyboardAccessoryOnlyFeaturesParam[] = {
        {kDisableKeyboardAccessoryParam,
         kDisableKeyboardAccessoryOnlyFeatures}};

constexpr FeatureEntry::FeatureParam
    kDisableKeyboardAccessoryCompletelyParam[] = {
        {kDisableKeyboardAccessoryParam, kDisableKeyboardAccessoryCompletely}};

constexpr FeatureEntry::FeatureVariation kDisableKeyboardAccessoryVariations[] =
    {{"A) only show symbols", kDisableKeyboardAccessoryOnlySymbolsParam,
      nullptr},
     {"B) only show lens and voice search",
      kDisableKeyboardAccessoryOnlyFeaturesParam, nullptr},
     {"C) disable completely", kDisableKeyboardAccessoryCompletelyParam,
      nullptr}};

constexpr FeatureEntry::FeatureParam
    kEnableFuseboxKeyboardAccessoryOnlySymbolsParam[] = {
        {kEnableFuseboxKeyboardAccessoryParam,
         kEnableFuseboxKeyboardAccessoryOnlySymbols}};

constexpr FeatureEntry::FeatureParam
    kEnableFuseboxKeyboardAccessoryOnlyFeaturesParam[] = {
        {kEnableFuseboxKeyboardAccessoryParam,
         kEnableFuseboxKeyboardAccessoryOnlyFeatures}};

constexpr FeatureEntry::FeatureParam
    kEnableFuseboxKeyboardAccessoryBothParam[] = {
        {kEnableFuseboxKeyboardAccessoryParam,
         kEnableFuseboxKeyboardAccessoryBoth}};

constexpr FeatureEntry::FeatureVariation
    kEnableFuseboxKeyboardAccessoryVariations[] = {
        {"A) only show symbols",
         kEnableFuseboxKeyboardAccessoryOnlySymbolsParam, nullptr},
        {"B) only show lens and voice search",
         kEnableFuseboxKeyboardAccessoryOnlyFeaturesParam, nullptr},
        {"C) enable both", kEnableFuseboxKeyboardAccessoryBothParam, nullptr}};

constexpr FeatureEntry::Choice kOmniboxTruncateSearchUrlChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled, switches::kEnableFeatures,
     "TruncateSearchUrlOq,TruncateSearchUrlQ"},
    {"Enabled Oq30kQ30k (30k oq, 30k q)", switches::kEnableFeatures,
     "TruncateSearchUrlOq:truncate_search_url_oq_length/30720,"
     "TruncateSearchUrlQ:truncate_search_url_q_length/30720"},
    {"Enabled Oq1kQ59k (1k oq, 59k q)", switches::kEnableFeatures,
     "TruncateSearchUrlOq:truncate_search_url_oq_length/1024,"
     "TruncateSearchUrlQ:truncate_search_url_q_length/60416"},
    {"Enabled Oq2kQ58k (2k oq, 58k q)", switches::kEnableFeatures,
     "TruncateSearchUrlOq:truncate_search_url_oq_length/2048,"
     "TruncateSearchUrlQ:truncate_search_url_q_length/59392"},
    {"Enabled Oq1kQNoTruncation (1k oq, q disabled)", switches::kEnableFeatures,
     "TruncateSearchUrlOq:truncate_search_url_oq_length/1024"},
    {"Enabled Oq2kQNoTruncation (2k oq, q disabled)", switches::kEnableFeatures,
     "TruncateSearchUrlOq:truncate_search_url_oq_length/2048"},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kDisableFeatures,
     "TruncateSearchUrlOq,TruncateSearchUrlQ"},
};

constexpr FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches3[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "3"}};
constexpr FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches4[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "4"}};
constexpr FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches5[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "5"}};
constexpr FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches6[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}};
constexpr FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches8[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "8"}};
constexpr FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches10[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "10"}};
constexpr FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches12[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "12"}};

constexpr FeatureEntry::FeatureVariation
    kOmniboxUIMaxAutocompleteMatchesVariations[] = {
        {"3 matches", kOmniboxUIMaxAutocompleteMatches3, nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4, nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5, nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6, nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8, nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10, nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12, nullptr}};

constexpr FeatureEntry::FeatureParam kContentPushNotificationsEnabledPromo[] = {
    {kContentPushNotificationsExperimentType, "1"}};
constexpr FeatureEntry::FeatureParam
    kContentPushNotificationsEnabledSetupLists[] = {
        {kContentPushNotificationsExperimentType, "2"}};
constexpr FeatureEntry::FeatureParam
    kContentPushNotificationsEnabledProvisional[] = {
        {kContentPushNotificationsExperimentType, "3"}};
constexpr FeatureEntry::FeatureParam
    kContentPushNotificationsPromoRegistrationOnly[] = {
        {kContentPushNotificationsExperimentType, "5"}};
constexpr FeatureEntry::FeatureParam
    kContentPushNotificationsProvisionalRegistrationOnly[] = {
        {kContentPushNotificationsExperimentType, "6"}};
constexpr FeatureEntry::FeatureParam
    kContentPushNotificationsSetUpListRegistrationOnly[] = {
        {kContentPushNotificationsExperimentType, "7"}};

constexpr FeatureEntry::FeatureVariation kContentPushNotificationsVariations[] =
    {{"Promo", kContentPushNotificationsEnabledPromo, nullptr},
     {"Set up list", kContentPushNotificationsEnabledSetupLists, nullptr},
     {"Provisional Notification", kContentPushNotificationsEnabledProvisional,
      nullptr},
     {"Promo Registeration Only",
      kContentPushNotificationsPromoRegistrationOnly, nullptr},
     {"Provisional Notification Registeration Only",
      kContentPushNotificationsProvisionalRegistrationOnly, nullptr},
     {"Set up list Registeration Only",
      kContentPushNotificationsSetUpListRegistrationOnly, nullptr}};

#if BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
// Feed Background Refresh Feature Params.
constexpr FeatureEntry::FeatureParam kOneHourIntervalOneHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "false"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, /* 60*60= */ "3600"}};
constexpr FeatureEntry::FeatureParam kFourHourIntervalSixHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "false"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, /* 4*60*60= */ "14400"}};
constexpr FeatureEntry::FeatureParam kOneHourIntervalOneHourMaxAgeRecurring[] =
    {{kEnableServerDrivenBackgroundRefreshSchedule, "false"},
     {kEnableRecurringBackgroundRefreshSchedule, "true"},
     {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
     {kBackgroundRefreshIntervalInSeconds, /* 60*60= */ "3600"}};
constexpr FeatureEntry::FeatureParam kFourHourIntervalSixHourMaxAgeRecurring[] =
    {{kEnableServerDrivenBackgroundRefreshSchedule, "false"},
     {kEnableRecurringBackgroundRefreshSchedule, "true"},
     {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
     {kBackgroundRefreshIntervalInSeconds, /* 4*60*60= */ "14400"}};
constexpr FeatureEntry::FeatureParam kServerDrivenOneHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};
constexpr FeatureEntry::FeatureParam kServerDrivenOneHourMaxAgeRecurring[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "true"},
    {kMaxCacheAgeInSeconds, /*60*60*/ "3600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};
constexpr FeatureEntry::FeatureParam kServerDrivenSixHourMaxAgeOnce[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "false"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};
constexpr FeatureEntry::FeatureParam kServerDrivenSixHourMaxAgeRecurring[] = {
    {kEnableServerDrivenBackgroundRefreshSchedule, "true"},
    {kEnableRecurringBackgroundRefreshSchedule, "true"},
    {kMaxCacheAgeInSeconds, /*6*60*60*/ "21600"},
    {kBackgroundRefreshIntervalInSeconds, "0"}};

// Feed Background Refresh Feature Variations.
constexpr FeatureEntry::FeatureVariation kFeedBackgroundRefreshVariations[] = {
    {"1hr Interval 1hr Max Age Once", kOneHourIntervalOneHourMaxAgeOnce,
     nullptr},
    {"4hr Interval 6hr Max Age Once", kFourHourIntervalSixHourMaxAgeOnce,
     nullptr},
    {"1hr Interval 1hr Max Age Recurring",
     kOneHourIntervalOneHourMaxAgeRecurring, nullptr},
    {"4hr Interval 6hr Max Age Recurring",
     kFourHourIntervalSixHourMaxAgeRecurring, nullptr},
    {"Server Driven 1hr Max Age Once", kServerDrivenOneHourMaxAgeOnce, nullptr},
    {"Server Driven 1hr Max Age Recurring", kServerDrivenOneHourMaxAgeRecurring,
     nullptr},
    {"Server Driven 6hr Max Age Once", kServerDrivenSixHourMaxAgeOnce, nullptr},
    {"Server Driven 6hr Max Age Recurring", kServerDrivenSixHourMaxAgeRecurring,
     nullptr},
};
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)

// Download List UI feature flag parameters.
// IMPORTANT: These values must match DownloadListUIType enum in features.h
constexpr FeatureEntry::FeatureParam kDownloadListDefaultUIParam[] = {
    {kDownloadListUITypeParam, "0"}};
constexpr FeatureEntry::FeatureParam kDownloadListCustomUIParam[] = {
    {kDownloadListUITypeParam, "1"}};
constexpr FeatureEntry::FeatureVariation kDownloadListVariations[] = {
    {"Default UI", kDownloadListDefaultUIParam, nullptr},
    {"Custom UI", kDownloadListCustomUIParam, nullptr},
};

// Default browser promo refresh feature flag parameters.
constexpr FeatureEntry::FeatureParam kDefaultBrowserPictureInPictureArm1[] = {
    {kDefaultBrowserPictureInPictureParam,
     kDefaultBrowserPictureInPictureParamEnabled}};
constexpr FeatureEntry::FeatureParam kDefaultBrowserPictureInPictureArm2[] = {
    {kDefaultBrowserPictureInPictureParam,
     kDefaultBrowserPictureInPictureParamDisabledDefaultApps}};
constexpr FeatureEntry::FeatureParam kDefaultBrowserPictureInPictureArm3[] = {
    {kDefaultBrowserPictureInPictureParam,
     kDefaultBrowserPictureInPictureParamEnabledDefaultApps}};
constexpr FeatureEntry::FeatureVariation
    kDefaultBrowserPictureInPictureVariations[] = {
        {"Picture-in-picture instructions.",
         kDefaultBrowserPictureInPictureArm1, nullptr},
        {"No picture in picture, default apps destination.",
         kDefaultBrowserPictureInPictureArm2, nullptr},
        {"Picture-in-picture instructions, default apps destination.",
         kDefaultBrowserPictureInPictureArm3, nullptr},
};

constexpr FeatureEntry::FeatureParam
    kDefaultBrowserNonModalPromoStringsSkipCopyPaste[] = {
        {kDefaultBrowserNonModalPromoStringsParam, "1"}};
constexpr FeatureEntry::FeatureParam
    kDefaultBrowserNonModalPromoStringsFewerSteps[] = {
        {kDefaultBrowserNonModalPromoStringsParam, "2"}};
constexpr FeatureEntry::FeatureVariation
    kDefaultBrowserNonModalPromoStringsVariations[] = {
        {"Skip copy & paste", kDefaultBrowserNonModalPromoStringsSkipCopyPaste,
         nullptr},
        {"Fewer steps, faster links",
         kDefaultBrowserNonModalPromoStringsFewerSteps, nullptr},
};

constexpr FeatureEntry::FeatureParam kOmniboxPastePromoExperimentArm1[] = {
    {"arm", "1"}};
constexpr FeatureEntry::FeatureParam kOmniboxPastePromoExperimentArm2[] = {
    {"arm", "2"}};
constexpr FeatureEntry::FeatureParam kOmniboxPastePromoExperimentArm3[] = {
    {"arm", "3"}};
constexpr FeatureEntry::FeatureParam kOmniboxPastePromoExperimentArm4[] = {
    {"arm", "4"}};
constexpr FeatureEntry::FeatureParam kOmniboxPastePromoExperimentArm5[] = {
    {"arm", "5"}};
constexpr FeatureEntry::FeatureParam kOmniboxPastePromoExperimentArm6[] = {
    {"arm", "6"}};
constexpr FeatureEntry::FeatureParam kOmniboxPastePromoExperimentArm7[] = {
    {"arm", "7"}};
constexpr FeatureEntry::FeatureParam kOmniboxPastePromoExperimentArm8[] = {
    {"arm", "8"}};
constexpr FeatureEntry::FeatureParam kOmniboxPastePromoExperimentArm9[] = {
    {"arm", "9"}};
constexpr FeatureEntry::FeatureParam kOmniboxPastePromoExperimentArm10[] = {
    {"arm", "10"}};

constexpr FeatureEntry::FeatureVariation
    kOmniboxPastePromoExperimentVariations[] = {
        {"Arm 1", kOmniboxPastePromoExperimentArm1, nullptr},
        {"Arm 2", kOmniboxPastePromoExperimentArm2, nullptr},
        {"Arm 3", kOmniboxPastePromoExperimentArm3, nullptr},
        {"Arm 4", kOmniboxPastePromoExperimentArm4, nullptr},
        {"Arm 5", kOmniboxPastePromoExperimentArm5, nullptr},
        {"Arm 6", kOmniboxPastePromoExperimentArm6, nullptr},
        {"Arm 7", kOmniboxPastePromoExperimentArm7, nullptr},
        {"Arm 8", kOmniboxPastePromoExperimentArm8, nullptr},
        {"Arm 9", kOmniboxPastePromoExperimentArm9, nullptr},
        {"Arm 10", kOmniboxPastePromoExperimentArm10, nullptr},
};

constexpr FeatureEntry::FeatureParam kSettingsDefaultBrowserCardParam[] = {
    {kIOSSettingsDefaultBrowserPromoTypeParam, "0"}};
constexpr FeatureEntry::FeatureParam kSettingsDefaultBrowserCellParam[] = {
    {kIOSSettingsDefaultBrowserPromoTypeParam, "1"}};
constexpr FeatureEntry::FeatureVariation
    kIOSSettingsDefaultBrowserPromoV2Variations[] = {
        {"SettingsDefaultBrowserCard", kSettingsDefaultBrowserCardParam,
         nullptr},
        {"SettingsDefaultBrowserCell", kSettingsDefaultBrowserCellParam,
         nullptr},
};

constexpr FeatureEntry::FeatureParam kDefaultBrowserPromoDestinationParam[] = {
    {kDefaultBrowserPromoOverflowMenuTypeParam, "0"}};
constexpr FeatureEntry::FeatureParam kDefaultBrowserPromoShortcutsParam[] = {
    {kDefaultBrowserPromoOverflowMenuTypeParam, "1"}};
constexpr FeatureEntry::FeatureVariation
    kDefaultBrowserPromoOverflowMenuVariations[] = {
        {"Destination", kDefaultBrowserPromoDestinationParam, nullptr},
        {"Shortcuts", kDefaultBrowserPromoShortcutsParam, nullptr},
};

constexpr FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1300;0.14,1398;1,1422"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1400"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
constexpr FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingDemotedBy50[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1250;0.14,1348;1,1422"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1350"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
constexpr FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingPromotedBy50[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1350;0.14,1448;1,1472"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1450"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
constexpr FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingPromotedBy100[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,550;0.018,1400;0.14,1498;1,1522"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1500"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};
constexpr FeatureEntry::FeatureParam
    kMlUrlPiecewiseMappedSearchBlendingMobileMapping[] = {
        {"MlUrlPiecewiseMappedSearchBlending", "true"},
        {"MlUrlPiecewiseMappedSearchBlending_BreakPoints",
         "0,590;0.006,790;0.082,1290;0.443,1360;0.464,1400;0.987,1425;1,1530"},
        {"MlUrlPiecewiseMappedSearchBlending_GroupingThreshold", "1340"},
        {"MlUrlPiecewiseMappedSearchBlending_RelevanceBias", "0"}};

constexpr FeatureEntry::FeatureVariation
    kMlUrlPiecewiseMappedSearchBlendingVariations[] = {
        {"adjusted by 0", kMlUrlPiecewiseMappedSearchBlendingAdjustedBy0,
         nullptr},
        {"demoted by 50", kMlUrlPiecewiseMappedSearchBlendingDemotedBy50,
         nullptr},
        {"promoted by 50", kMlUrlPiecewiseMappedSearchBlendingPromotedBy50,
         nullptr},
        {"promoted by 100", kMlUrlPiecewiseMappedSearchBlendingPromotedBy100,
         nullptr},
        {"mobile mapping", kMlUrlPiecewiseMappedSearchBlendingMobileMapping,
         nullptr},
};

constexpr FeatureEntry::FeatureParam kOmniboxMlUrlScoringEnabledWithFixes[] = {
    {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
    {"MlUrlScoringShortcutDocumentSignals", "true"},
};
constexpr FeatureEntry::FeatureParam
    kOmniboxMlUrlScoringUnlimitedNumCandidates[] = {
        {"MlUrlScoringUnlimitedNumCandidates", "true"},
        {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
        {"MlUrlScoringShortcutDocumentSignals", "true"},
};
// Sets Bookmark(1), History Quick(4), History URL(8), Shortcuts(64),
// Document(512), and History Fuzzy(65536) providers max matches to 10.
constexpr FeatureEntry::FeatureParam
    kOmniboxMlUrlScoringMaxMatchesByProvider10[] = {
        {"MlUrlScoringMaxMatchesByProvider",
         "1:10,4:10,8:10,64:10,512:10,65536:10"},
        {"enable_scoring_signals_annotators_for_ml_scoring", "true"},
        {"MlUrlScoringShortcutDocumentSignals", "true"},
};

constexpr FeatureEntry::FeatureVariation kOmniboxMlUrlScoringVariations[] = {
    {"Enabled with fixes", kOmniboxMlUrlScoringEnabledWithFixes, nullptr},
    {"unlimited suggestion candidates",
     kOmniboxMlUrlScoringUnlimitedNumCandidates, nullptr},
    {"Increase provider max limit to 10",
     kOmniboxMlUrlScoringMaxMatchesByProvider10, nullptr},
};

constexpr FeatureEntry::FeatureParam kMlUrlSearchBlendingStable[] = {
    {"MlUrlSearchBlending_StableSearchBlending", "true"},
    {"MlUrlSearchBlending_MappedSearchBlending", "false"},
};
constexpr FeatureEntry::FeatureParam
    kMlUrlSearchBlendingMappedConservativeUrls[] = {
        {"MlUrlSearchBlending_StableSearchBlending", "false"},
        {"MlUrlSearchBlending_MappedSearchBlending", "true"},
        {"MlUrlSearchBlending_MappedSearchBlendingMin", "0"},
        {"MlUrlSearchBlending_MappedSearchBlendingMax", "2000"},
        {"MlUrlSearchBlending_MappedSearchBlendingGroupingThreshold", "1000"},
};
constexpr FeatureEntry::FeatureParam kMlUrlSearchBlendingMappedModerateUrls[] =
    {
        {"MlUrlSearchBlending_StableSearchBlending", "false"},
        {"MlUrlSearchBlending_MappedSearchBlending", "true"},
};
constexpr FeatureEntry::FeatureParam
    kMlUrlSearchBlendingMappedAggressiveUrls[] = {
        {"MlUrlSearchBlending_StableSearchBlending", "false"},
        {"MlUrlSearchBlending_MappedSearchBlending", "true"},
        {"MlUrlSearchBlending_MappedSearchBlendingMin", "1000"},
        {"MlUrlSearchBlending_MappedSearchBlendingMax", "4000"},
        {"MlUrlSearchBlending_MappedSearchBlendingGroupingThreshold", "1500"},
};

constexpr FeatureEntry::FeatureVariation kMlUrlSearchBlendingVariations[] = {
    {"Stable", kMlUrlSearchBlendingStable, nullptr},
    {"Mapped conservative urls", kMlUrlSearchBlendingMappedConservativeUrls,
     nullptr},
    {"Mapped moderate urls", kMlUrlSearchBlendingMappedModerateUrls, nullptr},
    {"Mapped aggressive urls", kMlUrlSearchBlendingMappedAggressiveUrls,
     nullptr},
};

constexpr FeatureEntry::FeatureVariation kUrlScoringModelVariations[] = {
    {"Small model", {}, "3379590"},
    {"Full model", {}, "3380197"},
};

// Soft Lock
constexpr FeatureEntry::FeatureParam kIOSSoftLockNoDelay[] = {
    {kIOSSoftLockBackgroundThresholdParam, "0m"},
};

constexpr FeatureEntry::FeatureVariation kIOSSoftLockVariations[] = {
    {" - No delay", kIOSSoftLockNoDelay, nullptr}};

constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanShortPeriodParam[] = {{"period-ms", "250"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanMediumPeriodParam[] = {{"period-ms", "500"}};
constexpr flags_ui::FeatureEntry::FeatureParam
    kAutofillThrottleDocFormScanLongPeriodParam[] = {{"period-ms", "1000"}};
constexpr flags_ui::FeatureEntry::FeatureVariation
    kAutofillThrottleDocFormScanVariations[] = {
        {"Short period", kAutofillThrottleDocFormScanShortPeriodParam, nullptr},
        {"Medium period", kAutofillThrottleDocFormScanMediumPeriodParam,
         nullptr},
        {"Long period", kAutofillThrottleDocFormScanLongPeriodParam, nullptr}};

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
         nullptr},
        {"Medium period", kAutofillThrottleFilteredDocFormScanMediumPeriodParam,
         nullptr},
        {"Long period", kAutofillThrottleFilteredDocFormScanLongPeriodParam,
         nullptr}};

constexpr FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm1[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "1"}};
constexpr FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm2[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "2"}};
constexpr FeatureEntry::FeatureParam kUpdatedFirstRunSequenceArm3[] = {
    {first_run::kUpdatedFirstRunSequenceParam, "3"}};

constexpr FeatureEntry::FeatureVariation kUpdatedFirstRunSequenceVariations[] =
    {{" - Default browser promo first", kUpdatedFirstRunSequenceArm1, nullptr},
     {" - Remove sign in & sync conditionally", kUpdatedFirstRunSequenceArm2,
      nullptr},
     {" - DB promo first and remove sign in & sync",
      kUpdatedFirstRunSequenceArm3, nullptr}};

constexpr FeatureEntry::FeatureParam kSlowFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "0"}};
constexpr FeatureEntry::FeatureParam kDefaultFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "1"}};
constexpr FeatureEntry::FeatureParam kFastFullscreenTransitionSpeed[] = {
    {kFullscreenTransitionSpeedParam, "2"}};

constexpr FeatureEntry::FeatureVariation kFullscreenTransitionVariations[] = {
    {"Slow speed", kSlowFullscreenTransitionSpeed, nullptr},
    {"Default speed", kDefaultFullscreenTransitionSpeed, nullptr},
    {"Fast speed", kFastFullscreenTransitionSpeed, nullptr}};

constexpr FeatureEntry::FeatureParam kFullscreenScrollThreshold1[] = {
    {web::features::kFullscreenScrollThresholdAmount, "1"}};
constexpr FeatureEntry::FeatureParam kFullscreenScrollThreshold5[] = {
    {web::features::kFullscreenScrollThresholdAmount, "5"}};
constexpr FeatureEntry::FeatureParam kFullscreenScrollThreshold10[] = {
    {web::features::kFullscreenScrollThresholdAmount, "10"}};
constexpr FeatureEntry::FeatureParam kFullscreenScrollThreshold20[] = {
    {web::features::kFullscreenScrollThresholdAmount, "20"}};
constexpr FeatureEntry::FeatureVariation
    kFullscreenScrollThresholdVariations[] = {
        {"1px", kFullscreenScrollThreshold1, nullptr},
        {"5px", kFullscreenScrollThreshold5, nullptr},
        {"10px", kFullscreenScrollThreshold10, nullptr},
        {"20px", kFullscreenScrollThreshold20, nullptr}};

constexpr FeatureEntry::FeatureParam kAnimatedDBPInFREWithActionButtons[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "0"}};
constexpr FeatureEntry::FeatureParam kAnimatedDBPInFREWithShowMeHow[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "1"}};
constexpr FeatureEntry::FeatureParam kAnimatedDBPInFREWithInstructions[] = {
    {first_run::kAnimatedDefaultBrowserPromoInFREExperimentType, "2"}};

constexpr FeatureEntry::FeatureVariation
    kAnimatedDefaultBrowserPromoInFREVariations[] = {
        {" - with Action Buttons", kAnimatedDBPInFREWithActionButtons, nullptr},
        {" - with Show Me How", kAnimatedDBPInFREWithShowMeHow, nullptr},
        {" - with Instructions", kAnimatedDBPInFREWithInstructions, nullptr}};

constexpr FeatureEntry::FeatureParam
    kAndroidSwitcherDBPInFRESegmentationSignal[] = {
        {first_run::kAndroidSwitcherDefaultBrowserPromoInFREParam,
         first_run::
             kAndroidSwitcherDefaultBrowserPromoInFREParamSegmentationSignal}};
constexpr FeatureEntry::FeatureParam
    kAndroidSwitcherDBPInFRESwitcherInfoSignal[] = {
        {first_run::kAndroidSwitcherDefaultBrowserPromoInFREParam,
         first_run::
             kAndroidSwitcherDefaultBrowserPromoInFREParamSwitcherInfoSignal}};

constexpr FeatureEntry::FeatureVariation
    kAndroidSwitcherDefaultBrowserPromoInFREVariations[] = {
        {" - Segmentation Signal", kAndroidSwitcherDBPInFRESegmentationSignal,
         nullptr},
        {" - Switcher Info Signal", kAndroidSwitcherDBPInFRESwitcherInfoSignal,
         nullptr}};

constexpr FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm2[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "2"}};
constexpr FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm3[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "3"}};
constexpr FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm4[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "4"}};
constexpr FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm5[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "5"}};
constexpr FeatureEntry::FeatureParam kBestFeaturesScreenInFirstRunArm6[] = {
    {first_run::kBestFeaturesScreenInFirstRunParam, "6"}};

constexpr FeatureEntry::FeatureVariation
    kBestFeaturesScreenInFirstRunVariations[] = {
        {" - Variant A: General screen, after DB promo",
         kUpdatedFirstRunSequenceArm1, nullptr},
        {" - Variant B: General screen, before DB promo",
         kBestFeaturesScreenInFirstRunArm2, nullptr},
        {" - Variant C: General screen with passwords item",
         kBestFeaturesScreenInFirstRunArm3, nullptr},
        {" - Variant D: Shopping users screen, variant C as fallback",
         kBestFeaturesScreenInFirstRunArm4, nullptr},
        {" - Variant E: Show screen to signed-in users only",
         kBestFeaturesScreenInFirstRunArm5, nullptr},
        {" - Variant F: Show address bar promo",
         kBestFeaturesScreenInFirstRunArm6, nullptr}};

constexpr FeatureEntry::FeatureParam
    kIOSOneTapMiniMapRestrictionCrossValidate[] = {
        {kIOSOneTapMiniMapRestrictionCrossValidateParamName, "true"}};
constexpr FeatureEntry::FeatureParam
    kIOSOneTapMiniMapRestrictionThreshold999[] = {
        {kIOSOneTapMiniMapRestrictionThreshholdParamName, "0.999"}};
constexpr FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionMinLength20[] =
    {{kIOSOneTapMiniMapRestrictionMinCharsParamName, "20"}};
constexpr FeatureEntry::FeatureParam
    kIOSOneTapMiniMapRestrictionMaxSections6[] = {
        {kIOSOneTapMiniMapRestrictionMaxSectionsParamName, "6"}};
constexpr FeatureEntry::FeatureParam kIOSOneTapMiniMapRestrictionLongWords4[] =
    {{kIOSOneTapMiniMapRestrictionLongestWordMinCharsParamName, "4"}};
constexpr FeatureEntry::FeatureParam
    kIOSOneTapMiniMapRestrictionMinAlphaNum60[] = {
        {kIOSOneTapMiniMapRestrictionMinAlphanumProportionParamName, "0.6"}};

constexpr FeatureEntry::FeatureVariation
    kIOSOneTapMiniMapRestrictionsVariations[] = {
        {"Revalidate with NSDataDetector",
         kIOSOneTapMiniMapRestrictionCrossValidate, nullptr},
        {"Confidence Level (0.999)", kIOSOneTapMiniMapRestrictionThreshold999,
         nullptr},
        {"Minimum address length (20 chars)",
         kIOSOneTapMiniMapRestrictionMinLength20, nullptr},
        {"Maximum sections (6)", kIOSOneTapMiniMapRestrictionMaxSections6,
         nullptr},
        {"Longest word length (4)", kIOSOneTapMiniMapRestrictionLongWords4,
         nullptr},
        {"Proportion of alnum chars (60%)",
         kIOSOneTapMiniMapRestrictionMinAlphaNum60, nullptr}};

constexpr FeatureEntry::FeatureParam kFeedSwipeInProductHelpStaticInFirstRun[] =
    {{kFeedSwipeInProductHelpArmParam, "1"}};
constexpr FeatureEntry::FeatureParam
    kFeedSwipeInProductHelpStaticInSecondRun[] = {
        {kFeedSwipeInProductHelpArmParam, "2"}};
constexpr FeatureEntry::FeatureParam kFeedSwipeInProductHelpAnimated[] = {
    {kFeedSwipeInProductHelpArmParam, "3"}};

constexpr FeatureEntry::FeatureVariation kFeedSwipeInProductHelpVariations[] = {
    {" - Static IPH after the FRE", kFeedSwipeInProductHelpStaticInFirstRun,
     nullptr},
    {"- Static IPH after the second run",
     kFeedSwipeInProductHelpStaticInSecondRun, nullptr},
    {"- Animated IPH", kFeedSwipeInProductHelpAnimated, nullptr}};

constexpr FeatureEntry::FeatureParam kWelcomeBackArm1[] = {
    {kWelcomeBackParam, "1"}};
constexpr FeatureEntry::FeatureParam kWelcomeBackArm2[] = {
    {kWelcomeBackParam, "2"}};
constexpr FeatureEntry::FeatureParam kWelcomeBackArm3[] = {
    {kWelcomeBackParam, "3"}};
constexpr FeatureEntry::FeatureParam kWelcomeBackArm4[] = {
    {kWelcomeBackParam, "4"}};

constexpr FeatureEntry::FeatureVariation kWelcomeBackVariations[] = {
    {" - Variant A: Basics with Locked Incognito", kWelcomeBackArm1, nullptr},
    {" - Variant B: Basics with Save & Autofill Passwords", kWelcomeBackArm2,
     nullptr},
    {" - Variant C: Productivity & Shopping", kWelcomeBackArm3, nullptr},
    {" - Variant D: Sign-in Benefits", kWelcomeBackArm4, nullptr},
};

constexpr FeatureEntry::FeatureParam kBackgroundRefreshRegressionTestControl[] =
    {{"regression_test_arm", "control"}};
constexpr FeatureEntry::FeatureParam
    kBackgroundRefreshRegressionTestBaseline[] = {
        {"regression_test_arm", "baseline"}};
constexpr FeatureEntry::FeatureParam
    kBackgroundRefreshRegressionTestShortPersistenceDelay[] = {
        {"regression_test_arm", "short-persistence-delay"}};
constexpr FeatureEntry::FeatureParam
    kBackgroundRefreshRegressionTestLongRefreshInterval[] = {
        {"regression_test_arm", "long-refresh-interval"}};
constexpr FeatureEntry::FeatureParam
    kBackgroundRefreshRegressionTestNoBeacon[] = {
        {"regression_test_arm", "no-beacon"}};

constexpr FeatureEntry::FeatureVariation
    kBackgroundRefreshRegressionTestVariations[] = {
        {"Control", kBackgroundRefreshRegressionTestControl, nullptr},
        {"Baseline", kBackgroundRefreshRegressionTestBaseline, nullptr},
        {"Short Persistence Delay",
         kBackgroundRefreshRegressionTestShortPersistenceDelay, nullptr},
        {"Long Refresh Interval",
         kBackgroundRefreshRegressionTestLongRefreshInterval, nullptr},
        {"No Beacon", kBackgroundRefreshRegressionTestNoBeacon, nullptr}};

constexpr FeatureEntry::FeatureParam kBestOfAppFREArm1[] = {{"variant", "1"}};
constexpr FeatureEntry::FeatureParam kBestOfAppFREArm2[] = {{"variant", "2"}};
constexpr FeatureEntry::FeatureParam kBestOfAppFREArm3[] = {{"variant", "3"}};
constexpr FeatureEntry::FeatureParam kBestOfAppFREArm4[] = {{"variant", "4"}};
constexpr FeatureEntry::FeatureParam kBestOfAppFREArm4Upload[] = {
    {"variant", "4"},
    {"manual_upload_uma", "true"}};

constexpr FeatureEntry::FeatureVariation kBestOfAppFREVariations[] = {
    {" - Variant A: Lens Interactive Promo", kBestOfAppFREArm1, nullptr},
    {" - Variant B: Lens Animated Promo", kBestOfAppFREArm2, nullptr},
    {" - Variant C: Best Features", kBestOfAppFREArm3, nullptr},
    {" - Variant D: Guided Tour", kBestOfAppFREArm4, nullptr},
    {" - Variant D: Guided Tour with manual metric upload",
     kBestOfAppFREArm4Upload, nullptr},
};

constexpr FeatureEntry::FeatureParam kSingleScreenForBWGPromoConsent[] = {
    {kBWGPromoConsentParams, "1"}};
constexpr FeatureEntry::FeatureParam kDoubleScreenForBWGPromoConsent[] = {
    {kBWGPromoConsentParams, "2"}};
constexpr FeatureEntry::FeatureParam kSkipBWGPromoConsent[] = {
    {kBWGPromoConsentParams, "3"}};
constexpr FeatureEntry::FeatureParam kForceBWGFirstTimeRun[] = {
    {kBWGPromoConsentParams, "4"}};
constexpr FeatureEntry::FeatureParam kSkipNewUserDelay[] = {
    {kBWGPromoConsentParams, "5"}};
constexpr FeatureEntry::FeatureParam kForceGeminiLiveFRE[] = {
    {kBWGPromoConsentParams, "6"}};

constexpr FeatureEntry::FeatureVariation kBWGPromoConsentVariations[] = {
    {"Single screen for BWG Promo Consent Flow",
     kSingleScreenForBWGPromoConsent, nullptr},
    {"Double screen for BWG Promo Consent Flow",
     kDoubleScreenForBWGPromoConsent, nullptr},
    {"Skip FRE", kSkipBWGPromoConsent, nullptr},
    {"Force FRE", kForceBWGFirstTimeRun, nullptr},
    {"Skip new user delay", kSkipNewUserDelay, nullptr},
    {"Force Live FRE", kForceGeminiLiveFRE, nullptr}};

constexpr FeatureEntry::FeatureParam kGeminiActorBackgroundingDisabled[] = {
    {kGeminiActorBackgroundingParam, "false"}};
constexpr FeatureEntry::FeatureVariation kGeminiActorVariations[] = {
    {"Backgrounding Disabled", kGeminiActorBackgroundingDisabled, nullptr}};

constexpr FeatureEntry::FeatureParam kGeminiQuizzesOpenInNewTab[] = {
    {kGeminiQuizzesOpenInNewTabParam, "true"}};
constexpr FeatureEntry::FeatureVariation kGeminiQuizzesVariations[] = {
    {"Open in new tab", kGeminiQuizzesOpenInNewTab, nullptr}};

constexpr FeatureEntry::FeatureParam
    kGeminiAureusForegroundQuotaRefreshEnabled[] = {
        {kGeminiAureusForegroundQuotaRefreshParam, "true"}};
constexpr FeatureEntry::FeatureVariation kGeminiAureusVariations[] = {
    {"Foreground Quota Refresh Enabled",
     kGeminiAureusForegroundQuotaRefreshEnabled, nullptr}};

constexpr FeatureEntry::FeatureParam kGeminiLiveSkipIntro[] = {
    {kGeminiLiveSkipIntroParam, "true"}};
constexpr FeatureEntry::FeatureParam kGeminiLiveDisableIPH[] = {
    {kGeminiLiveDisableIPHParam, "true"}};
constexpr FeatureEntry::FeatureParam kGeminiLiveSkipIntroAndDisableIPH[] = {
    {kGeminiLiveSkipIntroParam, "true"},
    {kGeminiLiveDisableIPHParam, "true"}};
constexpr FeatureEntry::FeatureVariation kGeminiLiveVariations[] = {
    {"Skip Intro", kGeminiLiveSkipIntro, nullptr},
    {"Disable IPH", kGeminiLiveDisableIPH, nullptr},
    {"Skip Intro and Disable IPH", kGeminiLiveSkipIntroAndDisableIPH, nullptr}};

constexpr FeatureEntry::FeatureParam kGeminiFREExperimentVisualRich[] = {
    {kGeminiFREExperimentParam, kGeminiFREExperimentParamVisualRich}};
constexpr FeatureEntry::FeatureParam
    kGeminiFREExperimentLightweightConvenience[] = {
        {kGeminiFREExperimentParam,
         kGeminiFREExperimentParamLightweightConvenience}};
constexpr FeatureEntry::FeatureParam
    kGeminiFREExperimentLightweightPageSharing[] = {
        {kGeminiFREExperimentParam,
         kGeminiFREExperimentParamLightweightPageSharing}};
constexpr FeatureEntry::FeatureParam kGeminiFREExperimentLightweightDiverse[] =
    {{kGeminiFREExperimentParam, kGeminiFREExperimentParamLightweightDiverse}};

constexpr FeatureEntry::FeatureVariation kGeminiFREExperimentVariations[] = {
    {"Visual Rich", kGeminiFREExperimentVisualRich, nullptr},
    {"Lightweight (Convenience)", kGeminiFREExperimentLightweightConvenience,
     nullptr},
    {"Lightweight (Page Sharing)", kGeminiFREExperimentLightweightPageSharing,
     nullptr},
    {"Lightweight (Diverse)", kGeminiFREExperimentLightweightDiverse, nullptr},
};

constexpr FeatureEntry::FeatureParam
    kGeminiExperimentalGuidedOnboardingForce[] = {
        {kGeminiExperimentalGuidedOnboardingForceParam, "true"}};

constexpr FeatureEntry::FeatureVariation
    kGeminiExperimentalGuidedOnboardingVariations[] = {
        {"Force Guided Onboarding", kGeminiExperimentalGuidedOnboardingForce,
         nullptr}};

constexpr FeatureEntry::FeatureParam kPageActionMenuDirectEntryPoint[] = {
    {kPageActionMenuDirectEntryPointParam, "true"},
};
constexpr FeatureEntry::FeatureVariation kPageActionMenuVariations[] = {
    {"Direct Entry Point", kPageActionMenuDirectEntryPoint, nullptr},
};

constexpr FeatureEntry::FeatureParam
    kProactiveSuggestionsFrameworkPopupBlockerParam[] = {
        {kProactiveSuggestionsFrameworkPopupBlocker, "true"}};
constexpr FeatureEntry::FeatureVariation
    kProactiveSuggestionsFrameworkVariations[] = {
        {"Popup Blocker", kProactiveSuggestionsFrameworkPopupBlockerParam,
         nullptr}};

// LINT.IfChange(DataSharingVersioningChoices)
constexpr FeatureEntry::Choice kDataSharingVersioningStateChoices[] = {
    {"Default", "", ""},
    {flag_descriptions::kDataSharingSharedDataTypesEnabled,
     switches::kEnableFeatures, "SharedDataTypesKillSwitch"},
    {flag_descriptions::kDataSharingSharedDataTypesEnabledWithUi,
     switches::kEnableFeatures,
     "SharedDataTypesKillSwitch,DataSharingEnableUpdateChromeUI"},
    {"Disabled", switches::kDisableFeatures,
     "SharedDataTypesKillSwitch, DataSharingEnableUpdateChromeUI"},
};
// LINT.ThenChange(//chrome/browser/about_flags.cc:DataSharingVersioningChoices)

constexpr FeatureEntry::FeatureParam kComposeboxDevToolsForceFailure[] = {
    {kForceUploadFailureParam, "true"}};
constexpr FeatureEntry::FeatureParam kComposeboxDevToolsSlowLoad[] = {
    {kImageLoadDelayMsParam, "1000"}};
constexpr FeatureEntry::FeatureParam kComposeboxDevToolsSlowUpload[] = {
    {kUploadDelayMsParam, "3000"}};

constexpr FeatureEntry::FeatureVariation kComposeboxDevToolsVariations[] = {
    {"Force Failure", kComposeboxDevToolsForceFailure, nullptr},
    {"Slow Load (1s)", kComposeboxDevToolsSlowLoad, nullptr},
    {"Slow Upload (3s)", kComposeboxDevToolsSlowUpload, nullptr}};

constexpr FeatureEntry::FeatureParam
    kComposeboxConditionalPlusButtonHidePreEdit[] = {
        {kComposeboxConditionalPlusButtonParam, "1"}};

constexpr FeatureEntry::FeatureVariation
    kComposeboxConditionalPlusButtonVariations[] = {
        {"(Hide Plus button in pre-edit)",
         kComposeboxConditionalPlusButtonHidePreEdit, nullptr}};

constexpr FeatureEntry::FeatureParam kMobilePromoOnDesktopLens[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "1"},
    {kMobilePromoOnDesktopNotificationParam, "false"}};
constexpr FeatureEntry::FeatureParam kMobilePromoOnDesktopLensNotification[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "1"},
    {kMobilePromoOnDesktopNotificationParam, "true"}};
constexpr FeatureEntry::FeatureParam kMobilePromoOnDesktopESB[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "2"},
    {kMobilePromoOnDesktopNotificationParam, "false"}};
constexpr FeatureEntry::FeatureParam kMobilePromoOnDesktopESBNotification[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "2"},
    {kMobilePromoOnDesktopNotificationParam, "true"}};
constexpr FeatureEntry::FeatureParam kMobilePromoOnDesktopAutofill[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "3"},
    {kMobilePromoOnDesktopNotificationParam, "false"}};
constexpr FeatureEntry::FeatureParam
    kMobilePromoOnDesktopAutofillNotification[] = {
        {kMobilePromoOnDesktopPromoTypeParam, "3"},
        {kMobilePromoOnDesktopNotificationParam, "true"}};
constexpr FeatureEntry::FeatureParam kMobilePromoOnDesktopTabGroups[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "4"},
    {kMobilePromoOnDesktopNotificationParam, "false"}};
constexpr FeatureEntry::FeatureParam
    kMobilePromoOnDesktopTabGroupsNotification[] = {
        {kMobilePromoOnDesktopPromoTypeParam, "4"},
        {kMobilePromoOnDesktopNotificationParam, "true"}};
constexpr FeatureEntry::FeatureParam kMobilePromoOnDesktopPriceTracking[] = {
    {kMobilePromoOnDesktopPromoTypeParam, "5"},
    {kMobilePromoOnDesktopNotificationParam, "false"}};
constexpr FeatureEntry::FeatureParam
    kMobilePromoOnDesktopPriceTrackingNotification[] = {
        {kMobilePromoOnDesktopPromoTypeParam, "5"},
        {kMobilePromoOnDesktopNotificationParam, "true"}};

constexpr FeatureEntry::FeatureVariation kMobilePromoOnDesktopVariations[] = {
    {" - ESB", kMobilePromoOnDesktopESB, nullptr},
    {" - ESB with push notification", kMobilePromoOnDesktopESBNotification,
     nullptr},
    {" - PW Autofill", kMobilePromoOnDesktopAutofill, nullptr},
    {" - PW Autofill with push notification",
     kMobilePromoOnDesktopAutofillNotification, nullptr},
};

constexpr FeatureEntry::FeatureVariation
    kMobilePromoOnDesktopWave1Variations[] = {
        {" - Lens Promo", kMobilePromoOnDesktopLens, nullptr},
        {" - Lens Promo with push notification",
         kMobilePromoOnDesktopLensNotification, nullptr},
        {" - Tab Groups", kMobilePromoOnDesktopTabGroups, nullptr},
        {" - Tab Groups with push notification",
         kMobilePromoOnDesktopTabGroupsNotification, nullptr},
        {" - Price Tracking", kMobilePromoOnDesktopPriceTracking, nullptr},
        {" - Price Tracking with push notification",
         kMobilePromoOnDesktopPriceTrackingNotification, nullptr},
};

constexpr FeatureEntry::FeatureParam kWalletApiPrivatePassesUrl[] = {
    {"wallet_pass_save_url", "https://wallet1ppasses.pa.googleapis.com"}};

constexpr FeatureEntry::FeatureVariation
    kWalletApiPrivatePassesEnabledVariations[] = {
        {"1P URL", kWalletApiPrivatePassesUrl, nullptr}};

constexpr FeatureEntry::FeatureParam
    kPersistTabContextFileSystem_WasHidden_FullContext[] = {
        {kPersistTabContextStorageParam, "0"},
        {kPersistTabContextExtractionTimingParam, "0"},
        {kPersistTabContextDataParam, "0"}};
constexpr FeatureEntry::FeatureParam
    kPersistTabContextSqlite_WasHidden_FullContext[] = {
        {kPersistTabContextStorageParam, "1"},
        {kPersistTabContextExtractionTimingParam, "0"},
        {kPersistTabContextDataParam, "0"}};
constexpr FeatureEntry::FeatureParam
    kPersistTabContextSqlite_WasHiddenPageLoad_FullContext[] = {
        {kPersistTabContextStorageParam, "1"},
        {kPersistTabContextExtractionTimingParam, "1"},
        {kPersistTabContextDataParam, "0"}};
constexpr FeatureEntry::FeatureParam
    kPersistTabContextSqlite_WasHidden_InnerTextOnly[] = {
        {kPersistTabContextStorageParam, "1"},
        {kPersistTabContextExtractionTimingParam, "0"},
        {kPersistTabContextDataParam, "1"}};

constexpr FeatureEntry::FeatureVariation kPersistTabContextVariations[] = {
    {"FileSystem, On Tab Hide, APC + Inner Text",
     kPersistTabContextFileSystem_WasHidden_FullContext, nullptr},
    {"SQLite, On Tab Hide, APC + Inner Text",
     kPersistTabContextSqlite_WasHidden_FullContext, nullptr},
    {"SQLite, On Tab Hide & Page Load, APC + Inner Text",
     kPersistTabContextSqlite_WasHiddenPageLoad_FullContext, nullptr},
    {"SQLite, On Tab Hide, Inner Text",
     kPersistTabContextSqlite_WasHidden_InnerTextOnly, nullptr}};

constexpr FeatureEntry::FeatureParam kIOSExpandedSetupListSafariImport[] = {
    {kIOSExpandedSetupListVariationParam,
     kIOSExpandedSetupListVariationParamSafariImport}};

constexpr FeatureEntry::FeatureParam
    kIOSExpandedSetupListBackgroundCustomization[] = {
        {kIOSExpandedSetupListVariationParam,
         kIOSExpandedSetupListVariationParamBackgroundCustomization}};

constexpr FeatureEntry::FeatureParam
    IOSExpandedSetupListVariationParamAllExceptCPE[] = {
        {kIOSExpandedSetupListVariationParam,
         kIOSExpandedSetupListVariationParamAllExceptCPE}};

constexpr FeatureEntry::FeatureParam kIOSExpandedSetupListAll[] = {
    {kIOSExpandedSetupListVariationParam,
     kIOSExpandedSetupListVariationParamAll}};

constexpr FeatureEntry::FeatureVariation kIOSExpandedSetupListVariations[] = {
    {"Safari Data Import", kIOSExpandedSetupListSafariImport, nullptr},
    {"Home Background Customization",
     kIOSExpandedSetupListBackgroundCustomization, nullptr},
    {"Safari Data Import & Home Background Customization (without CPE)",
     IOSExpandedSetupListVariationParamAllExceptCPE, nullptr},
    {"Safari Data Import, Home Background Customization, CPE",
     kIOSExpandedSetupListAll, nullptr}};

constexpr FeatureEntry::FeatureParam kModelBasedPageClassificationParam1[] = {
    {kModelBasedPageClassificationExecutionRateParam, "1"}};
constexpr FeatureEntry::FeatureParam kModelBasedPageClassificationParam10[] = {
    {kModelBasedPageClassificationExecutionRateParam, "10"}};
constexpr FeatureEntry::FeatureParam kModelBasedPageClassificationParam25[] = {
    {kModelBasedPageClassificationExecutionRateParam, "25"}};
constexpr FeatureEntry::FeatureParam kModelBasedPageClassificationParam50[] = {
    {kModelBasedPageClassificationExecutionRateParam, "50"}};
constexpr FeatureEntry::FeatureParam kModelBasedPageClassificationParam75[] = {
    {kModelBasedPageClassificationExecutionRateParam, "75"}};
constexpr FeatureEntry::FeatureParam kModelBasedPageClassificationParam100[] = {
    {kModelBasedPageClassificationExecutionRateParam, "100"}};

constexpr FeatureEntry::FeatureVariation
    kModelBasedPageClassificationVariations[] = {
        {"(1%)", kModelBasedPageClassificationParam1, nullptr},
        {"(10%)", kModelBasedPageClassificationParam10, nullptr},
        {"(25%)", kModelBasedPageClassificationParam25, nullptr},
        {"(50%)", kModelBasedPageClassificationParam50, nullptr},
        {"(75%)", kModelBasedPageClassificationParam75, nullptr},
        {"(100%)", kModelBasedPageClassificationParam100, nullptr},
};

constexpr FeatureEntry::FeatureParam kAfterEditForExplainGeminiEditMenu[] = {
    {kExplainGeminiEditMenuParams, "1"}};
constexpr FeatureEntry::FeatureParam kAfterSearchForExplainGeminiEditMenu[] = {
    {kExplainGeminiEditMenuParams, "2"}};
constexpr FeatureEntry::FeatureParam kAdjacentForExplainGeminiEditMenu[] = {
    {kExplainGeminiEditMenuParams, "3"}};

constexpr FeatureEntry::FeatureVariation kPositionForExplainGeminiEditMenu[] = {
    {"Explain Gemini shows up after Edit", kAfterEditForExplainGeminiEditMenu,
     nullptr},
    {"Explain Gemini shows up after Search with Google",
     kAfterSearchForExplainGeminiEditMenu, nullptr},
    {"Explain Gemini shows up adjacent to Search with Google",
     kAdjacentForExplainGeminiEditMenu, nullptr}};

constexpr FeatureEntry::FeatureParam kPageActionMenuIconSparkles1[] = {
    {kPageActionMenuIconParams, "1"}};
constexpr FeatureEntry::FeatureParam kPageActionMenuIconSparkles2[] = {
    {kPageActionMenuIconParams, "2"}};

constexpr FeatureEntry::FeatureVariation kPageActionMenuIconVariations[] = {
    {"Sparkles 1", kPageActionMenuIconSparkles1, nullptr},
    {"Sparkles 2", kPageActionMenuIconSparkles2, nullptr}};

constexpr FeatureEntry::FeatureParam kAssistantContainerParamDebugMode[] = {
    {kAssistantContainerParam, kAssistantContainerParamDebug}};

constexpr FeatureEntry::FeatureParam kAssistantContainer30[] = {
    {kAssistantContainerMediumDetentPercentParam, "30"}};

constexpr FeatureEntry::FeatureParam kAssistantContainer60[] = {
    {kAssistantContainerMediumDetentPercentParam, "60"}};

constexpr FeatureEntry::FeatureVariation kAssistantContainerVariations[] = {
    {"with debug enabled", kAssistantContainerParamDebugMode, nullptr},
    {"30% medium detent", kAssistantContainer30, nullptr},
    {"60% medium detent", kAssistantContainer60, nullptr}};

constexpr FeatureEntry::FeatureParam kAutoSubmissionDismissThenSubmit[] = {
    {"auto-submission-type", "DismissThenSubmit"},
};
constexpr FeatureEntry::FeatureParam kAutoSubmissionSubmitThenDismiss[] = {
    {"auto-submission-type", "SubmitThenDismiss"},
};
constexpr FeatureEntry::FeatureParam
    kAutoSubmissionDismissThenBlockThenSubmit[] = {
        {"auto-submission-type", "DismissThenBlockThenSubmit"},
};
constexpr FeatureEntry::FeatureParam
    kAutoSubmissionDismissThenBlockThenSubmitWithWait[] = {
        {"auto-submission-type", "DismissThenBlockThenSubmit"},
        {"auto-submission-use-wait-period", "100"},
};
constexpr FeatureEntry::FeatureParam
    kAutoSubmissionDismissThenSubmitWithWait[] = {
        {"auto-submission-type", "DismissThenSubmit"},
        {"auto-submission-use-wait-period", "100"},
};
constexpr FeatureEntry::FeatureParam kAutoSubmissionScriptSubmit[] = {
    {"auto-submission-type", "ScriptSubmit"},
};

constexpr FeatureEntry::FeatureVariation kAutoSubmissionVariations[] = {
    {"Dismiss then Submit", kAutoSubmissionDismissThenSubmit, nullptr},
    {"Submit then Dismiss", kAutoSubmissionSubmitThenDismiss, nullptr},
    {"Dismiss then Block then Submit",
     kAutoSubmissionDismissThenBlockThenSubmit, nullptr},
    {"Dismiss then Block then Submit (Wait)",
     kAutoSubmissionDismissThenBlockThenSubmitWithWait, nullptr},
    {"Dismiss then Submit (Wait)", kAutoSubmissionDismissThenSubmitWithWait,
     nullptr},
    {"Script Submit", kAutoSubmissionScriptSubmit, nullptr},
};

constexpr FeatureEntry::FeatureParam kCrossDeviceSigninDefaultUrl[] = {
    {"url", "https://www.google.com/chrome/go-mobile"},
};

constexpr FeatureEntry::FeatureVariation kCrossDeviceSigninVariations[] = {
    {"Default URL (https://www.google.com/chrome/go-mobile)",
     kCrossDeviceSigninDefaultUrl, nullptr},
};

constexpr FeatureEntry::FeatureParam kNewTabPageUICleanupTightPadding[] = {
    {kNewTabPageUICleanupArmParam, "1"}};
constexpr FeatureEntry::FeatureParam kNewTabPageUICleanupMediumPadding[] = {
    {kNewTabPageUICleanupArmParam, "2"}};
constexpr FeatureEntry::FeatureParam kNewTabPageUICleanupPreferredPadding[] = {
    {kNewTabPageUICleanupArmParam, "3"}};
constexpr FeatureEntry::FeatureParam
    kNewTabPageUICleanupFakeboxBackgroundAndShadow[] = {
        {kNewTabPageUICleanupArmParam, "4"}};

constexpr FeatureEntry::FeatureVariation kNewTabPageUICleanupVariations[] = {
    {" - Tight Padding", kNewTabPageUICleanupTightPadding, nullptr},
    {" - Medium Padding", kNewTabPageUICleanupMediumPadding, nullptr},
    {" - Preferred Padding", kNewTabPageUICleanupPreferredPadding, nullptr},
    {" - Fakebox Background and Shadow Update",
     kNewTabPageUICleanupFakeboxBackgroundAndShadow, nullptr},
};

constexpr FeatureEntry::FeatureParam kAim3pEntrypointDebugEnabled[] = {
    {"Aim3pEntrypointDebug", "true"}};

constexpr FeatureEntry::FeatureVariation kAim3pEntrypointVariations[] = {
    {"with debug config", kAim3pEntrypointDebugEnabled, nullptr}};

constexpr FeatureEntry::FeatureParam
    kAimButtonRefactorFocusComposeboxAimQuickAction[] = {
        {kAimButtonRefactorArmParam, "1"}};
constexpr FeatureEntry::FeatureParam
    kAimButtonRefactorImageGenerationQuickAction[] = {
        {kAimButtonRefactorArmParam, "2"}};
constexpr FeatureEntry::FeatureParam
    kAimButtonRefactorAttachImageQuickAction[] = {
        {kAimButtonRefactorArmParam, "3"}};
constexpr FeatureEntry::FeatureParam kAimButtonRefactorAimAsModule[] = {
    {kAimButtonRefactorArmParam, "4"}};
constexpr FeatureEntry::FeatureParam kAimButtonRefactorAimAsMvt[] = {
    {kAimButtonRefactorArmParam, "5"}};
constexpr FeatureEntry::FeatureParam kAimButtonRefactorNoChips[] = {
    {kAimButtonRefactorArmParam, "6"}};

constexpr FeatureEntry::FeatureVariation kAimButtonRefactorVariations[] = {
    {" - Focus Composebox AIM Quick Action",
     kAimButtonRefactorFocusComposeboxAimQuickAction, nullptr},
    {" - Image Generation Quick Action",
     kAimButtonRefactorImageGenerationQuickAction, nullptr},
    {" - Attach Image Quick Action", kAimButtonRefactorAttachImageQuickAction,
     nullptr},
    {" - AIM as Module", kAimButtonRefactorAimAsModule, nullptr},
    {" - AIM as MVT", kAimButtonRefactorAimAsMvt, nullptr},
    {" - No Chips", kAimButtonRefactorNoChips, nullptr},
};

constexpr FeatureEntry::FeatureParam
    kGeminiContextualSuggestionsCuesWithOnDeviceClassifierCPU[] = {
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesOnDeviceClassifierParam,
         "true"},
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesAllowGpuExecutionParam,
         "false"},
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesTitleAndUrlOnlyParam,
         "true"},
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesServerModelExecutionParam,
         "true"}};
constexpr FeatureEntry::FeatureParam
    kGeminiContextualSuggestionsCuesWithOnDeviceClassifierGPU[] = {
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesOnDeviceClassifierParam,
         "true"},
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesAllowGpuExecutionParam,
         "true"},
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesTitleAndUrlOnlyParam,
         "true"},
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesServerModelExecutionParam,
         "true"}};
constexpr FeatureEntry::FeatureParam
    kGeminiContextualSuggestionsCuesWithOnDeviceClassifierApcCPU[] = {
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesOnDeviceClassifierParam,
         "true"},
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesAllowGpuExecutionParam,
         "false"},
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesTitleAndUrlOnlyParam,
         "false"}};
constexpr FeatureEntry::FeatureParam
    kGeminiContextualSuggestionsCuesWithOnDeviceClassifierApcGPU[] = {
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesOnDeviceClassifierParam,
         "true"},
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesAllowGpuExecutionParam,
         "true"},
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesTitleAndUrlOnlyParam,
         "false"}};
constexpr FeatureEntry::FeatureParam
    kGeminiContextualSuggestionsCuesIgnoreThresholds[] = {
        {contextual_cueing::
             kGeminiContextualSuggestionsCuesIgnoreThresholdsParam,
         "true"}};

constexpr FeatureEntry::FeatureVariation
    kGeminiContextualSuggestionsCuesVariations[] = {
        {"with on-device classifier (CPU - Title and URL)",
         kGeminiContextualSuggestionsCuesWithOnDeviceClassifierCPU, nullptr},
        {"with on-device classifier (GPU / ANE - Title and URL)",
         kGeminiContextualSuggestionsCuesWithOnDeviceClassifierGPU, nullptr},
        {"with on-device classifier (CPU - APC and Passages)",
         kGeminiContextualSuggestionsCuesWithOnDeviceClassifierApcCPU, nullptr},
        {"with on-device classifier (GPU / ANE - APC and Passages)",
         kGeminiContextualSuggestionsCuesWithOnDeviceClassifierApcGPU, nullptr},
        {"with thresholds ignored",
         kGeminiContextualSuggestionsCuesIgnoreThresholds, nullptr}};

constexpr FeatureEntry::FeatureParam
    kGeminiContextualSuggestionsCuesUiModeInfobarThenChip[] = {
        {contextual_cueing::kGeminiContextualSuggestionsCuesUiModeParam,
         "infobar_then_chip"}};
constexpr FeatureEntry::FeatureParam
    kGeminiContextualSuggestionsCuesUiModeInfobarOnly[] = {
        {contextual_cueing::kGeminiContextualSuggestionsCuesUiModeParam,
         "infobar_only"}};
constexpr FeatureEntry::FeatureParam
    kGeminiContextualSuggestionsCuesUiModeOmniboxChipOnly[] = {
        {contextual_cueing::kGeminiContextualSuggestionsCuesUiModeParam,
         "omnibox_chip_only"}};

constexpr FeatureEntry::FeatureVariation
    kGeminiContextualSuggestionsCuesUiModeVariations[] = {
        {"Infobar Then Omnibox Chip",
         kGeminiContextualSuggestionsCuesUiModeInfobarThenChip, nullptr},
        {"Infobar Only", kGeminiContextualSuggestionsCuesUiModeInfobarOnly,
         nullptr},
        {"Omnibox Chip Only",
         kGeminiContextualSuggestionsCuesUiModeOmniboxChipOnly, nullptr}};

// The choice order mirrors the layout previously generated by
// `FEATURE_WITH_PARAMS_VALUE_TYPE` (Default, Enabled, variations, Disabled) so
// that choice indices already persisted in users' flag state keep their
// meaning.
// Image embedding matching rolls out together with base CSD, so it is enabled
// by every enabled choice.
constexpr FeatureEntry::Choice kClientSideDetectionIosChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {flags_ui::kGenericExperimentChoiceEnabled, switches::kEnableFeatures,
     "ClientSideDetectionEnabledIos,"
     "ClientSideDetectionImageEmbeddingMatch"},
    {"Enabled without enforcement", switches::kEnableFeatures,
     "ClientSideDetectionEnabledIos:CsdEnforceIos/false,"
     "ClientSideDetectionImageEmbeddingMatch"},
    {"Enabled with enforcement", switches::kEnableFeatures,
     "ClientSideDetectionEnabledIos:CsdEnforceIos/true,"
     "ClientSideDetectionImageEmbeddingMatch"},
    {flags_ui::kGenericExperimentChoiceDisabled, switches::kDisableFeatures,
     "ClientSideDetectionEnabledIos"},
};

constexpr FeatureEntry::FeatureParam kPageClassificationOnDeviceOnly[] = {
    {kPageClassificationModeParam, "on_device_only"}};
constexpr FeatureEntry::FeatureParam kPageClassificationVerticalsOnly[] = {
    {kPageClassificationModeParam, "verticals_only"}};
constexpr FeatureEntry::FeatureParam kPageClassificationOnDeviceWithFallback[] =
    {{kPageClassificationModeParam, "on_device_with_fallback"}};

constexpr FeatureEntry::FeatureVariation kPageClassificationVariations[] = {
    {"On-Device Only", kPageClassificationOnDeviceOnly, nullptr},
    {"Verticals Only", kPageClassificationVerticalsOnly, nullptr},
    {"On-Device with Verticals Fallback",
     kPageClassificationOnDeviceWithFallback, nullptr}};

constexpr FeatureEntry::FeatureParam
    kMagicStackNTPThemeTipIosNonActionableParams[] = {
        {segmentation_platform::features::
             kMagicStackNTPThemeTipIosActionableParam,
         "false"}};
constexpr FeatureEntry::FeatureParam
    kMagicStackNTPThemeTipIosActionableParam[] = {
        {segmentation_platform::features::
             kMagicStackNTPThemeTipIosActionableParam,
         "true"}};
constexpr FeatureEntry::FeatureVariation kMagicStackNTPThemeTipIosVariations[] =
    {
        {"(Non-Actionable)", kMagicStackNTPThemeTipIosNonActionableParams,
         nullptr},
        {"(Actionable)", kMagicStackNTPThemeTipIosActionableParam, nullptr},
};

constexpr FeatureEntry::FeatureParam
    kMagicStackNTPCustomizationTipIosNonActionableParams[] = {
        {segmentation_platform::features::
             kMagicStackNTPCustomizationTipIosActionableParam,
         "false"}};
constexpr FeatureEntry::FeatureParam
    kMagicStackNTPCustomizationTipIosActionableParam[] = {
        {segmentation_platform::features::
             kMagicStackNTPCustomizationTipIosActionableParam,
         "true"}};
constexpr FeatureEntry::FeatureVariation
    kMagicStackNTPCustomizationTipIosVariations[] = {
        {"(Non-Actionable)",
         kMagicStackNTPCustomizationTipIosNonActionableParams, nullptr},
        {"(Actionable)", kMagicStackNTPCustomizationTipIosActionableParam,
         nullptr},
};

constexpr FeatureEntry::FeatureParam
    kMagicStackTipsNotificationTipIosNonActionableParams[] = {
        {segmentation_platform::features::
             kMagicStackTipsNotificationTipIosActionableParam,
         "false"}};
constexpr FeatureEntry::FeatureParam
    kMagicStackTipsNotificationTipIosActionableParam[] = {
        {segmentation_platform::features::
             kMagicStackTipsNotificationTipIosActionableParam,
         "true"}};
constexpr FeatureEntry::FeatureVariation
    kMagicStackTipsNotificationTipIosVariations[] = {
        {"(Non-Actionable)",
         kMagicStackTipsNotificationTipIosNonActionableParams, nullptr},
        {"(Actionable)", kMagicStackTipsNotificationTipIosActionableParam,
         nullptr},
};

constexpr FeatureEntry::FeatureParam kLensSearchTipShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kLensEphemeralModuleSearchVariation},
};
constexpr FeatureEntry::FeatureParam kLensShopTipShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kLensEphemeralModuleShopVariation},
};
constexpr FeatureEntry::FeatureParam kLensTranslateTipShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kLensEphemeralModuleTranslateVariation},
};
constexpr FeatureEntry::FeatureParam kAddressBarPositionTipShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kAddressBarPositionEphemeralModule},
};
constexpr FeatureEntry::FeatureParam kSavePasswordsTipShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kSavePasswordsEphemeralModule},
};
constexpr FeatureEntry::FeatureParam kAutofillPasswordsTipShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kAutofillPasswordsEphemeralModule},
};
constexpr FeatureEntry::FeatureParam kEnhancedSafeBrowsingTipShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kEnhancedSafeBrowsingEphemeralModule},
};

constexpr FeatureEntry::FeatureParam kNTPThemeTipShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kNTPThemeEphemeralModule},
};

constexpr FeatureEntry::FeatureParam kTipsNotificationTipShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kTipsNotificationsEphemeralModule},
};

constexpr FeatureEntry::FeatureParam kNTPCustomizationTipShowArm[] = {
    {segmentation_platform::features::kEphemeralCardRankerForceShowCardParam,
     segmentation_platform::kNTPCustomizationEphemeralModule},
};

constexpr FeatureEntry::FeatureVariation
    kEphemeralCardRankerCardOverrideOptions[] = {
        {"- Force Show Lens Search Tip", kLensSearchTipShowArm, nullptr},
        {"- Force Show Lens Shop Tip", kLensShopTipShowArm, nullptr},
        {"- Force Show Lens Translate Tip", kLensTranslateTipShowArm, nullptr},
        {"- Force Show Address Bar Position Tip", kAddressBarPositionTipShowArm,
         nullptr},
        {"- Force Show Save Passwords Tip", kSavePasswordsTipShowArm, nullptr},
        {"- Force Show Autofill Passwords Tip", kAutofillPasswordsTipShowArm,
         nullptr},
        {"- Force Show Enhanced Safe Browsing Tip",
         kEnhancedSafeBrowsingTipShowArm, nullptr},
        {"- Force Show NTP Theme Tip", kNTPThemeTipShowArm, nullptr},
        {"- Force Show Tips Notification Tip", kTipsNotificationTipShowArm,
         nullptr},
        {"- Force Show NTP Customization Tip", kNTPCustomizationTipShowArm,
         nullptr},
};

constexpr FeatureEntry::FeatureParam kEnterpriseProxyErrorForced403[] = {
    {enterprise_net::kForcedDisguisedErrorCodeParamName, "403"}};
constexpr FeatureEntry::FeatureParam kEnterpriseProxyErrorForced500[] = {
    {enterprise_net::kForcedDisguisedErrorCodeParamName, "500"}};
constexpr FeatureEntry::FeatureParam kEnterpriseProxyErrorForced502[] = {
    {enterprise_net::kForcedDisguisedErrorCodeParamName, "502"}};
constexpr FeatureEntry::FeatureParam kEnterpriseProxyErrorForced503[] = {
    {enterprise_net::kForcedDisguisedErrorCodeParamName, "503"}};
constexpr FeatureEntry::FeatureParam kEnterpriseProxyErrorForced504[] = {
    {enterprise_net::kForcedDisguisedErrorCodeParamName, "504"}};

constexpr FeatureEntry::FeatureParam
    kEnterpriseProxyErrorForcedUnsupported429[] = {
        {enterprise_net::kForcedDisguisedErrorCodeParamName, "429"}};

constexpr FeatureEntry::FeatureParam
    kEnterpriseProxyErrorForcedSignInRequired[] = {
        {enterprise_net::kForceSignInRequiredParamName, "true"}};

constexpr FeatureEntry::FeatureVariation
    kEnterpriseProxyErrorHandlingVariations[] = {
        {"Force 403 (Authorization / Block)", kEnterpriseProxyErrorForced403,
         nullptr},
        {"Force 500 (Internal Server Error)", kEnterpriseProxyErrorForced500,
         nullptr},
        {"Force 502 (Bad Gateway)", kEnterpriseProxyErrorForced502, nullptr},
        {"Force 503 (Service Unavailable)", kEnterpriseProxyErrorForced503,
         nullptr},
        {"Force 504 (Gateway Timeout)", kEnterpriseProxyErrorForced504,
         nullptr},
        {"Force Unsupported HTTP Code (429)",
         kEnterpriseProxyErrorForcedUnsupported429, nullptr},
        {"Force Sign-in Required (Authentication)",
         kEnterpriseProxyErrorForcedSignInRequired, nullptr},
};

constexpr FeatureEntry::FeatureParam
    kEnableAccountPreviewSwitchingAccountSkipCooldownParams[] = {
        {"SkipCooldown", "true"}};

constexpr FeatureEntry::FeatureVariation
    kEnableAccountPreviewSwitchingAccountVariations[] = {
        {"(Skip Cooldown)",
         kEnableAccountPreviewSwitchingAccountSkipCooldownParams, nullptr}};

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
// All FeatureEntry tables (kFeatureEntries and the Choice, FeatureParam and
// FeatureVariation arrays it references) must be constexpr to guarantee they
// don't introduce static initializers. For example, parameter names must be
// string literals or char arrays: reading base::FeatureParam::name from an
// extern param is not a constant expression.
//
// When adding a new choice, add it to the end of the list.
constexpr auto kFeatureEntries = std::to_array<flags_ui::FeatureEntry>({
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
    {"wallet-api-private-passes-enabled",
     flag_descriptions::kWalletApiPrivatePassesEnabledName,
     flag_descriptions::kWalletApiPrivatePassesEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         wallet::features::kWalletApiPrivatePassesEnabled,
         kWalletApiPrivatePassesEnabledVariations,
         "WalletApiPrivatePassesEnabled")},
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
         autofill::features::debug::kAutofillShowTypePredictions)},
    {"ntp-background-downsample-image",
     flag_descriptions::kNTPBackgroundDownsampleImageName,
     flag_descriptions::kNTPBackgroundDownsampleImageDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNTPBackgroundDownsampleImage)},
    {"ntp-alpha-background-collections",
     flag_descriptions::kNtpAlphaBackgroundCollectionsName,
     flag_descriptions::kNtpAlphaBackgroundCollectionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(ntp_features::kNtpAlphaBackgroundCollections)},
    {"new-tab-page-redesign", flag_descriptions::kNewTabPageRedesignName,
     flag_descriptions::kNewTabPageRedesignDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewTabPageRedesign)},
    {"ntp-mvt-in-bottom-sheet", flag_descriptions::kMVTInBottomSheetName,
     flag_descriptions::kMVTInBottomSheetDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMVTInBottomSheet)},
    {"plus-button-in-fakebox", flag_descriptions::kPlusButtonInFakeboxName,
     flag_descriptions::kPlusButtonInFakeboxDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPlusButtonInFakebox)},
    {"plus-button-menu-in-fakebox",
     flag_descriptions::kPlusButtonMenuInFakeboxName,
     flag_descriptions::kPlusButtonMenuInFakeboxDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPlusButtonMenuInFakebox)},
    {"plus-button-menu-more-options-submenu",
     flag_descriptions::kPlusButtonMenuMoreOptionsSubmenuName,
     flag_descriptions::kPlusButtonMenuMoreOptionsSubmenuDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kPlusButtonMenuMoreOptionsSubmenu)},
    {"plus-button-menu-string-updates",
     flag_descriptions::kPlusButtonMenuStringUpdatesName,
     flag_descriptions::kPlusButtonMenuStringUpdatesDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kPlusButtonMenuStringUpdates)},
    {"plus-button-menu-vertical-list-view",
     flag_descriptions::kPlusButtonMenuVerticalListViewName,
     flag_descriptions::kPlusButtonMenuVerticalListViewDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kPlusButtonMenuVerticalListView)},
    {"fullscreen-viewport-adjustment-experiment",
     flag_descriptions::kFullscreenSmoothScrollingName,
     flag_descriptions::kFullscreenSmoothScrollingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kSmoothScrollingDefault)},
    {"fullscreen-scroll-threshold",
     flag_descriptions::kFullscreenScrollThresholdName,
     flag_descriptions::kFullscreenScrollThresholdDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(web::features::kFullscreenScrollThreshold,
                                    kFullscreenScrollThresholdVariations,
                                    "FullscreenScrollThreshold")},
    {"webpage-alternative-text-zoom",
     flag_descriptions::kWebPageAlternativeTextZoomName,
     flag_descriptions::kWebPageAlternativeTextZoomDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(web::kWebPageAlternativeTextZoom)},
    {"webpage-text-zoom-ipad", flag_descriptions::kWebPageTextZoomIPadName,
     flag_descriptions::kWebPageTextZoomIPadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageTextZoomIPad)},
    {"aim-server-eligibility",
     flag_descriptions::kIOSOmniboxAimServerEligibilityName,
     flag_descriptions::kIOSOmniboxAimServerEligibilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kAimServerEligibilityEnabled)},
    {"omnibox-truncate-search-url",
     flag_descriptions::kOmniboxTruncateSearchUrlName,
     flag_descriptions::kOmniboxTruncateSearchUrlDescription, flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kOmniboxTruncateSearchUrlChoices)},
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
    {"omnibox-cross-device-tab-zero-suggest",
     flag_descriptions::kOmniboxCrossDeviceTabZeroSuggestName,
     flag_descriptions::kOmniboxCrossDeviceTabZeroSuggestDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kOmniboxCrossDeviceTabZeroSuggest)},
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kForceStartupSigninPromo)},
    {"shared-highlighting-ios", flag_descriptions::kSharedHighlightingIOSName,
     flag_descriptions::kSharedHighlightingIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSharedHighlightingIOS)},
    {"snapshot-background-decode",
     flag_descriptions::kSnapshotBackgroundDecodeName,
     flag_descriptions::kSnapshotBackgroundDecodeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSnapshotBackgroundDecode)},
    {"snapshot-concurrent-access",
     flag_descriptions::kSnapshotConcurrentAccessName,
     flag_descriptions::kSnapshotConcurrentAccessDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSnapshotConcurrentAccess)},
    {"snapshot-compressed-jpeg-quality",
     flag_descriptions::kSnapshotCompressedJPEGQualityName,
     flag_descriptions::kSnapshotCompressedJPEGQualityDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kSnapshotCompressedJPEGQuality)},
    {"snapshot-downsample-image",
     flag_descriptions::kSnapshotDownsampleImageName,
     flag_descriptions::kSnapshotDownsampleImageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSnapshotDownsampleImage)},
    {"ios-expanded-setup-list", flag_descriptions::kIOSExpandedSetupListName,
     flag_descriptions::kIOSExpandedSetupListDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSExpandedSetupList,
                                    kIOSExpandedSetupListVariations,
                                    "IOSExpandedSetupList")},
    {"ios-expanded-tips", flag_descriptions::kIOSExpandedTipsName,
     flag_descriptions::kIOSExpandedTipsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSExpandedTips)},
    {"one-time-default-browser-notification",
     flag_descriptions::kIOSOneTimeDefaultBrowserNotificationName,
     flag_descriptions::kIOSOneTimeDefaultBrowserNotificationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSOneTimeDefaultBrowserNotification)},
    {"ios-provides-app-notification-settings",
     flag_descriptions::kIOSProvidesAppNotificationSettingsName,
     flag_descriptions::kIOSProvidesAppNotificationSettingsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSProvidesAppNotificationSettings)},
    {"wait-threshold-seconds-for-capabilities-api",
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiName,
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kWaitThresholdMillisecondsForCapabilitiesApiChoices)},
    {"consistent-logo-doodle-height",
     flag_descriptions::kConsistentLogoDoodleHeightName,
     flag_descriptions::kConsistentLogoDoodleHeightDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kConsistentLogoDoodleHeight)},
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
    {"enable-lens-in-omnibox-copied-image",
     flag_descriptions::kEnableLensInOmniboxCopiedImageName,
     flag_descriptions::kEnableLensInOmniboxCopiedImageDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableLensInOmniboxCopiedImage)},
    {"enable-lens-view-finder-unified-experience",
     flag_descriptions::kEnableLensViewFinderUnifiedExperienceName,
     flag_descriptions::kEnableLensViewFinderUnifiedExperienceDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLensViewFinderUnifiedExperience)},
    {"price-insights", commerce::flag_descriptions::kPriceInsightsName,
     commerce::flag_descriptions::kPriceInsightsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kPriceInsights)},
    {"optimization-guide-debug-logs",
     flag_descriptions::kOptimizationGuideDebugLogsName,
     flag_descriptions::kOptimizationGuideDebugLogsDescription,
     flags_ui::kOsIos,
     SINGLE_VALUE_TYPE(optimization_guide::kDebugLoggingEnabledSwitch)},
    {"intents-on-measurements", flag_descriptions::kMeasurementsName,
     flag_descriptions::kMeasurementsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableMeasurements)},
    {"omnibox-https-upgrades", flag_descriptions::kOmniboxHttpsUpgradesName,
     flag_descriptions::kOmniboxHttpsUpgradesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kDefaultTypedNavigationsToHttps)},
    {"enable-feed-ablation", flag_descriptions::kEnableFeedAblationName,
     flag_descriptions::kEnableFeedAblationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableFeedAblation)},
    {"ios-proactive-password-generation-bottom-sheet",
     flag_descriptions::kIOSProactivePasswordGenerationBottomSheetName,
     flag_descriptions::kIOSProactivePasswordGenerationBottomSheetDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kIOSProactivePasswordGenerationBottomSheet)},
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
    {"omnibox-on-device-tail-suggestions",
     flag_descriptions::kOmniboxOnDeviceTailSuggestionsName,
     flag_descriptions::kOmniboxOnDeviceTailSuggestionsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kOnDeviceTailModel)},
    {"omnibox-prerender-on-touch-down",
     flag_descriptions::kOmniboxPrerenderOnTouchDownName,
     flag_descriptions::kOmniboxPrerenderOnTouchDownDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kOmniboxPrerenderOnTouchDown)},
    {"omnibox-most-visited-tiles-horizontal-render-group",
     flag_descriptions::kMostVisitedTilesHorizontalRenderGroupName,
     flag_descriptions::kMostVisitedTilesHorizontalRenderGroupDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kMostVisitedTilesHorizontalRenderGroup)},
    {"metrickit-non-crash-reports",
     flag_descriptions::kMetrickitNonCrashReportName,
     flag_descriptions::kMetrickitNonCrashReportDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMetrickitNonCrashReport)},
    {"default-browser-off-cycle-promo",
     flag_descriptions::kDefaultBrowserOffCyclePromoName,
     flag_descriptions::kDefaultBrowserOffCyclePromoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSDefaultBrowserOffCyclePromo)},
    {"use-scene-view-controller",
     flag_descriptions::kUseSceneViewControllerName,
     flag_descriptions::kUseSceneViewControllerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseSceneViewController)},
    {"display-tracing", flag_descriptions::kDisplayTracingName,
     flag_descriptions::kDisplayTracingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDisplayTracing)},
    {"default-browser-non-modal-promo-strings",
     flag_descriptions::kDefaultBrowserNonModalPromoStringsName,
     flag_descriptions::kDefaultBrowserNonModalPromoStringsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kDefaultBrowserNonModalPromoStrings,
         kDefaultBrowserNonModalPromoStringsVariations,
         "DefaultBrowserNonModalPromoStrings")},
    {"default-browser-picture-in-picture",
     flag_descriptions::kDefaultBrowserPictureInPictureName,
     flag_descriptions::kDefaultBrowserPictureInPictureDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDefaultBrowserPictureInPicture,
                                    kDefaultBrowserPictureInPictureVariations,
                                    "DefaultBrowserPictureInPicture")},
    {"default-browser-pip-text-video",
     flag_descriptions::kDefaultBrowserPipTextVideoName,
     flag_descriptions::kDefaultBrowserPipTextVideoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDefaultBrowserPipTextVideo)},
    {"omnibox-paste-promo-experiment",
     flag_descriptions::kOmniboxPastePromoExperimentName,
     flag_descriptions::kOmniboxPastePromoExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kOmniboxPastePromoExperiment,
                                    kOmniboxPastePromoExperimentVariations,
                                    "OmniboxPastePromoExperiment")},
#if BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"feed-background-refresh-ios",
     flag_descriptions::kFeedBackgroundRefreshName,
     flag_descriptions::kFeedBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFeedBackgroundRefresh,
                                    kFeedBackgroundRefreshVariations,
                                    "FeedBackgroundRefresh")},
#endif  // BUILDFLAG(IOS_BACKGROUND_MODE_ENABLED)
    {"shopping-list", commerce::flag_descriptions::kShoppingListName,
     commerce::flag_descriptions::kShoppingListDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(commerce::kShoppingList)},
    {"ios-gemini-bottom-sheet-migration",
     flag_descriptions::kIOSGeminiBottomSheetMigrationName,
     flag_descriptions::kIOSGeminiBottomSheetMigrationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSGeminiBottomSheetMigration)},
    {"ios-browser-edit-menu-metrics",
     flag_descriptions::kIOSBrowserEditMenuMetricsName,
     flag_descriptions::kIOSBrowserEditMenuMetricsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSBrowserEditMenuMetrics)},
    {"omnibox-grouping-framework-non-zps",
     flag_descriptions::kOmniboxGroupingFrameworkForTypedSuggestionsName,
     flag_descriptions::kOmniboxGroupingFrameworkForTypedSuggestionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kGroupingFrameworkForNonZPS)},
    {"bwg-precise-location", flag_descriptions::kGeminiPreciseLocationName,
     flag_descriptions::kGeminiPreciseLocationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiPreciseLocation)},
    {"ai-hub-new-badge", flag_descriptions::kAIHubNewBadgeName,
     flag_descriptions::kAIHubNewBadgeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAIHubNewBadge)},
    {"autofill-payments-field-swapping",
     flag_descriptions::kAutofillPaymentsFieldSwappingName,
     flag_descriptions::kAutofillPaymentsFieldSwappingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillPaymentsFieldSwapping)},
    {"grey-image-cifilter", flag_descriptions::kGreyImageCIFilterName,
     flag_descriptions::kGreyImageCIFilterDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGreyImageCIFilter)},
    {"https-upgrades-ios", flag_descriptions::kHttpsUpgradesName,
     flag_descriptions::kHttpsUpgradesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(security_interstitials::features::kHttpsUpgrades)},
    {"disable-lens-camera", flag_descriptions::kDisableLensCameraName,
     flag_descriptions::kDisableLensCameraDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDisableLensCamera)},
    {"enable-lens-on-ipad", flag_descriptions::kEnableLensOnIPadName,
     flag_descriptions::kEnableLensOnIPadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLensOnIPad)},
    {"search-engine-choice-screen-snackbar",
     flag_descriptions::kSearchEngineChoiceScreenSnackbarName,
     flag_descriptions::kSearchEngineChoiceScreenSnackbarDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kSearchEngineChoiceScreenSnackbar)},
    {"send-tab-to-self-enhanced-handoff",
     flag_descriptions::kSendTabToSelfEnhancedHandoffName,
     flag_descriptions::kSendTabToSelfEnhancedHandoffDescription,
     flags_ui::kOsIos, MULTI_VALUE_TYPE(kSendTabToSelfEnhancedHandoffChoices)},
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
    {"app-background-refresh-ios", flag_descriptions::kAppBackgroundRefreshName,
     flag_descriptions::kAppBackgroundRefreshDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableAppBackgroundRefresh)},
    {"background-refresh-regression-test",
     flag_descriptions::kBackgroundRefreshRegressionTestName,
     flag_descriptions::kBackgroundRefreshRegressionTestDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBackgroundRefreshRegressionTest,
                                    kBackgroundRefreshRegressionTestVariations,
                                    "BackgroundRefreshRegressionTest")},
    {"autofill-reject-form-submissions-without-user-gesture",
     flag_descriptions::kAutofillRejectFormSubmissionsWithoutUserGestureName,
     flag_descriptions::
         kAutofillRejectFormSubmissionsWithoutUserGestureDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillRejectFormSubmissionsWithoutUserGesture)},
    {"autofill-support-date-input",
     flag_descriptions::kAutofillSupportDateInputName,
     flag_descriptions::kAutofillSupportDateInputDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillSupportDateInput)},
    {"enable-trait-collection-registration",
     flag_descriptions::kEnableTraitCollectionRegistrationName,
     flag_descriptions::kEnableTraitCollectionRegistrationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableTraitCollectionRegistration)},
    {"lens-overlay-enable-landscape-compatibility",
     flag_descriptions::kLensOverlayEnableLandscapeCompatibilityName,
     flag_descriptions::kLensOverlayEnableLandscapeCompatibilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensOverlayEnableLandscapeCompatibility)},
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
    {"lens-bypass-compression-for-c2pa",
     flag_descriptions::kLensBypassCompressionForC2paName,
     flag_descriptions::kLensBypassCompressionForC2paDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(lens::features::kLensBypassCompressionForC2pa)},
    {"collaboration-messaging", flag_descriptions::kCollaborationMessagingName,
     flag_descriptions::kCollaborationMessagingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(collaboration::features::kCollaborationMessaging)},
    {"update-tab-group-colors", flag_descriptions::kUpdateTabGroupColorsName,
     flag_descriptions::kUpdateTabGroupColorsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(tab_groups::kUpdateTabGroupColors)},
    {"updated-fre-screens-sequence", flag_descriptions::kUpdatedFRESequenceName,
     flag_descriptions::kUpdatedFRESequenceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(first_run::kUpdatedFirstRunSequence,
                                    kUpdatedFirstRunSequenceVariations,
                                    "UpdatedFirstRunSequence")},
    {"download-auto-deletion", flag_descriptions::kDownloadAutoDeletionName,
     flag_descriptions::kDownloadAutoDeletionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDownloadAutoDeletionFeatureEnabled)},
    {"download-auto-deletion-clear-files-on-every-startup",
     flag_descriptions::kDownloadAutoDeletionClearFilesOnEveryStartupName,
     flag_descriptions::
         kDownloadAutoDeletionClearFilesOnEveryStartupDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDownloadAutoDeletionClearFilesOnEveryStartup)},
    {"download-list-ios", flag_descriptions::kDownloadListName,
     flag_descriptions::kDownloadListDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDownloadList,
                                    kDownloadListVariations,
                                    "IOSDownloadList")},
    {"animated-default-browser-promo-in-fre",
     flag_descriptions::kAnimatedDefaultBrowserPromoInFREName,
     flag_descriptions::kAnimatedDefaultBrowserPromoInFREDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         first_run::kAnimatedDefaultBrowserPromoInFRE,
         kAnimatedDefaultBrowserPromoInFREVariations,
         "AnimatedDBPInFRE-Layout")},
    {"fullscreen-transition-speed",
     flag_descriptions::kFullscreenTransitionSpeedName,
     flag_descriptions::kFullscreenTransitionSpeedDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kFullscreenTransitionSpeed,
                                    kFullscreenTransitionVariations,
                                    "IOSFullscreenTransitionSpeed")},
    {"refactor-toolbars-size", flag_descriptions::kRefactorToolbarsSizeName,
     flag_descriptions::kRefactorToolbarsSizeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kRefactorToolbarsSize)},
    {"ios-best-features-screen",
     flag_descriptions::kBestFeaturesScreenInFirstRunName,
     flag_descriptions::kBestFeaturesScreenInFirstRunDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(first_run::kBestFeaturesScreenInFirstRun,
                                    kBestFeaturesScreenInFirstRunVariations,
                                    "BestFeaturesScreenInFirstRun")},
    {"ios-one-tap-mini-map-restrictions",
     flag_descriptions::kIOSOneTapMiniMapRestrictionsName,
     flag_descriptions::kIOSOneTapMiniMapRestrictionsDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSOneTapMiniMapRestrictions,
                                    kIOSOneTapMiniMapRestrictionsVariations,
                                    "IOSOneTapMiniMapRestrictions")},
    {"enhanced-calendar", flag_descriptions::kEnhancedCalendarName,
     flag_descriptions::kEnhancedCalendarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnhancedCalendar)},
    {"explain-gemini-edit-menu", flag_descriptions::kExplainGeminiEditMenuName,
     flag_descriptions::kExplainGeminiEditMenuDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kExplainGeminiEditMenu,
                                    kPositionForExplainGeminiEditMenu,
                                    "ExplainGeminiEditMenu")},
    {"data-sharing-debug-logs", flag_descriptions::kDataSharingDebugLogsName,
     flag_descriptions::kDataSharingDebugLogsDescription, flags_ui::kOsIos,
     SINGLE_VALUE_TYPE(data_sharing::kDataSharingDebugLoggingEnabled)},
    {"feed-swipe-iph", flag_descriptions::kFeedSwipeInProductHelpName,
     flag_descriptions::kFeedSwipeInProductHelpDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kFeedSwipeInProductHelp,
                                    kFeedSwipeInProductHelpVariations,
                                    "FeedSwipeInProductHelp")},
    {"notification-collision-management",
     flag_descriptions::kNotificationCollisionManagementName,
     flag_descriptions::kNotificationCollisionManagementDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kNotificationCollisionManagement)},
    {"lens-overlay-navigation-history",
     flag_descriptions::kLensOverlayNavigationHistoryName,
     flag_descriptions::kLensOverlayNavigationHistoryDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayNavigationHistory)},
    {"lens-overlay-custom-bottom-sheet",
     flag_descriptions::kLensOverlayCustomBottomSheetName,
     flag_descriptions::kLensOverlayCustomBottomSheetDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOverlayCustomBottomSheet)},
    {"page-action-menu", flag_descriptions::kPageActionMenuName,
     flag_descriptions::kPageActionMenuDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kPageActionMenu,
                                    kPageActionMenuVariations,
                                    "IOSPageActionMenu")},
    {"page-action-menu-auth-flow",
     flag_descriptions::kPageActionMenuAuthFlowName,
     flag_descriptions::kPageActionMenuAuthFlowDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPageActionMenuAuthFlow)},
    {"generalized-gemini-entry-flow",
     flag_descriptions::kGeneralizedGeminiEntryFlowName,
     flag_descriptions::kGeneralizedGeminiEntryFlowDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kGeneralizedGeminiEntryFlow)},
    {"gemini-aureus", flag_descriptions::kGeminiAureusName,
     flag_descriptions::kGeminiAureusDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kGeminiAureus,
                                    kGeminiAureusVariations,
                                    "GeminiAureus")},
    {"gemini-luminous", flag_descriptions::kGeminiLuminousName,
     flag_descriptions::kGeminiLuminousDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiLuminous)},
    {"app-switcher-ai-summarization",
     flag_descriptions::kAppSwitcherAISummarizationName,
     flag_descriptions::kAppSwitcherAISummarizationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAppSwitcherAISummarization)},
    {"proactive-suggestions-framework",
     flag_descriptions::kProactiveSuggestionsFrameworkName,
     flag_descriptions::kProactiveSuggestionsFrameworkDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kProactiveSuggestionsFramework,
                                    kProactiveSuggestionsFrameworkVariations,
                                    "ProactiveSuggestionsFramework")},
    {"ask-gemini-chip-ignore-criteria",
     flag_descriptions::kAskGeminiChipIgnoreCriteriaName,
     flag_descriptions::kAskGeminiChipIgnoreCriteriaDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAskGeminiChipIgnoreCriteria)},
    {"bwg-promo-consent", flag_descriptions::kBWGPromoConsentName,
     flag_descriptions::kBWGPromoConsentDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBWGPromoConsent,
                                    kBWGPromoConsentVariations,
                                    "IOSBWGPromoConsent")},
    {"safe-browsing-local-lists-use-sbv5",
     flag_descriptions::kSafeBrowsingLocalListsUseSBv5Name,
     flag_descriptions::kSafeBrowsingLocalListsUseSBv5Description,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(safe_browsing::kLocalListsUseSBv5)},
    {"safe-browsing-trusted-url",
     flag_descriptions::kSafeBrowsingTrustedURLName,
     flag_descriptions::kSafeBrowsingTrustedURLDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSafeBrowsingTrustedURL)},
    {"ios-welcome-back-screen", flag_descriptions::kWelcomeBackName,
     flag_descriptions::kWelcomeBackDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kWelcomeBack,
                                    kWelcomeBackVariations,
                                    "WelcomeBack")},
    {"best-of-app-fre", flag_descriptions::kBestOfAppFREName,
     flag_descriptions::kBestOfAppFREDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBestOfAppFRE,
                                    kBestOfAppFREVariations,
                                    "BestOfAppFRE")},
    {"use-new-feed-eligibility-service",
     flag_descriptions::kUseFeedEligibilityServiceName,
     flag_descriptions::kUseFeedEligibilityServiceDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseFeedEligibilityService)},
    {"lens-load-aim-in-lens-result-page",
     flag_descriptions::kLensLoadAIMInLensResultPageName,
     flag_descriptions::kLensLoadAIMInLensResultPageDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensLoadAIMInLensResultPage)},
    {"disable-autofill-strike-system",
     flag_descriptions::kDisableAutofillStrikeSystemName,
     flag_descriptions::kDisableAutofillStrikeSystemDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(strike_database::features::kDisableStrikeSystem)},
    {"ios-default-browser-promo-propensity-model",
     flag_descriptions::kDefaultBrowserPromoPropensityModelName,
     flag_descriptions::kDefaultBrowserPromoPropensityModelDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         segmentation_platform::features::kDefaultBrowserPromoPropensityModel)},
    {"ios-default-browser-contextual-promo",
     flag_descriptions::kIOSDefaultBrowserContextualPromoName,
     flag_descriptions::kIOSDefaultBrowserContextualPromoDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSDefaultBrowserContextualPromo)},
    {"ios-settings-default-browser-promo-v2",
     flag_descriptions::kIOSSettingsDefaultBrowserPromoV2Name,
     flag_descriptions::kIOSSettingsDefaultBrowserPromoV2Description,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSSettingsDefaultBrowserPromoV2,
                                    kIOSSettingsDefaultBrowserPromoV2Variations,
                                    "IOSSettingsDefaultBrowserPromoV2")},
    {"default-browser-promo-overflow-menu",
     flag_descriptions::kDefaultBrowserPromoOverflowMenuName,
     flag_descriptions::kDefaultBrowserPromoOverflowMenuDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDefaultBrowserPromoOverflowMenu,
                                    kDefaultBrowserPromoOverflowMenuVariations,
                                    "DefaultBrowserPromoOverflowMenu")},
    {"taiyaki-all-surfaces", flag_descriptions::kTaiyakiAllSurfacesName,
     flag_descriptions::kTaiyakiAllSurfacesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kTaiyakiAllSurfaces)},
    {"lens-camera-no-still-output-required",
     flag_descriptions::kLensCameraNoStillOutputRequiredName,
     flag_descriptions::kLensCameraNoStillOutputRequiredDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensCameraNoStillOutputRequired)},
    {"lens-camera-unbinned-capture-formats-preferred",
     flag_descriptions::kLensCameraUnbinnedCaptureFormatsPreferredName,
     flag_descriptions::kLensCameraUnbinnedCaptureFormatsPreferredDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensCameraUnbinnedCaptureFormatsPreferred)},
    {"lens-continuous-zoom-enabled",
     flag_descriptions::kLensContinuousZoomEnabledName,
     flag_descriptions::kLensContinuousZoomEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensContinuousZoomEnabled)},
    {"lens-composebox-identity-delegation",
     flag_descriptions::kLensComposeboxIdentityDelegationName,
     flag_descriptions::kLensComposeboxIdentityDelegationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(lens::features::kLensComposeboxIdentityDelegation)},
    {"lens-initial-lvf-zoom-level-90-percent",
     flag_descriptions::kLensInitialLvfZoomLevel90PercentName,
     flag_descriptions::kLensInitialLvfZoomLevel90PercentDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensInitialLvfZoomLevel90Percent)},
    {"lens-triple-camera-enabled",
     flag_descriptions::kLensTripleCameraEnabledName,
     flag_descriptions::kLensTripleCameraEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensTripleCameraEnabled)},
    {"lens-use-separate-request-id-for-viewport-images",
     flag_descriptions::kLensUseSeparateRequestIdForViewportImagesName,
     flag_descriptions::kLensUseSeparateRequestIdForViewportImagesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         lens::features::kLensUseSeparateRequestIdForViewportImages)},
    {"lens-send-drive-tab-urls-in-composeboxes",
     flag_descriptions::kLensSendDriveTabUrlsInComposeboxesName,
     flag_descriptions::kLensSendDriveTabUrlsInComposeboxesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(lens::features::kLensSendDriveTabUrlsInComposeboxes)},
    // LINT.IfChange(DataSharingVersioning)
    {"shared-data-types-kill-switch",
     flag_descriptions::kDataSharingVersioningStatesName,
     flag_descriptions::kDataSharingVersioningStatesDescription,
     flags_ui::kOsIos, MULTI_VALUE_TYPE(kDataSharingVersioningStateChoices)},
    // LINT.ThenChange(//chrome/browser/about_flags.cc:DataSharingVersioning)
    {"ios-trusted-vault-notification",
     flag_descriptions::kIOSTrustedVaultNotificationName,
     flag_descriptions::kIOSTrustedVaultNotificationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSTrustedVaultNotification)},
    {"ios-skip-fre-default-browser-promo-in-eea",
     flag_descriptions::kSkipDefaultBrowserPromoInFirstRunName,
     flag_descriptions::kSkipDefaultBrowserPromoInFirstRunDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(first_run::kSkipDefaultBrowserPromoInFirstRun)},
    {"apply-clientside-model-predictions-for-password-types",
     flag_descriptions::kApplyClientsideModelPredictionsForPasswordTypesName,
     flag_descriptions::
         kApplyClientsideModelPredictionsForPasswordTypesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::
                            kApplyClientsideModelPredictionsForPasswordTypes)},
    {"cpe-passkey-largeblob-support",
     flag_descriptions::kCredentialProviderPasskeyLargeBlobName,
     flag_descriptions::kCredentialProviderPasskeyLargeBlobDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kCredentialProviderPasskeyLargeBlob)},
    {"autofill-save-card-bottom-sheet-strike-limit-ios",
     flag_descriptions::kAutofillSaveCardBottomSheetStrikeLimitIosName,
     flag_descriptions::kAutofillSaveCardBottomSheetStrikeLimitIosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSaveCardBottomSheetStrikeLimitIos)},
    {"composebox-deep-search", flag_descriptions::kComposeboxDeepSearchName,
     flag_descriptions::kComposeboxDeepSearchDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxDeepSearch)},
    {"composebox-server-side-state",
     flag_descriptions::kComposeboxServerSideStateName,
     flag_descriptions::kComposeboxServerSideStateDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxServerSideState)},
    {"composebox-devtools", flag_descriptions::kComposeboxDevToolsName,
     flag_descriptions::kComposeboxDevToolsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kComposeboxDevTools,
                                    kComposeboxDevToolsVariations,
                                    "ComposeboxDevTools")},
    {"context-management-in-composebox",
     flag_descriptions::kContextManagementInComposeboxName,
     flag_descriptions::kContextManagementInComposeboxDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kContextManagementInComposebox)},
    {"contextual-tasks", flag_descriptions::kContextualTasksName,
     flag_descriptions::kContextualTasksDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(contextual_tasks::kContextualTasks)},
    {"mobile-promo-on-desktop-data-collection",
     flag_descriptions::kMobilePromoOnDesktopRecordActiveDaysName,
     flag_descriptions::kMobilePromoOnDesktopRecordActiveDaysDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMobilePromoOnDesktopRecordActiveDays)},
    {"mobile-promo-on-desktop-with-reminder",
     flag_descriptions::kMobilePromoOnDesktopName,
     flag_descriptions::kMobilePromoOnDesktopDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kMobilePromoOnDesktopWithReminder,
                                    kMobilePromoOnDesktopVariations,
                                    "MobilePromoOnDesktopWithReminder")},
    {"autofill-bottom-sheet-new-blur",
     flag_descriptions::kAutofillBottomSheetNewBlurName,
     flag_descriptions::kAutofillBottomSheetNewBlurDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAutofillBottomSheetNewBlur)},
    {"hide-toolbar", flag_descriptions::kHideToolbarName,
     flag_descriptions::kHideToolbarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kHideToolbar)},
    {"smart-tab-grouping", flag_descriptions::kSmartTabGroupingName,
     flag_descriptions::kSmartTabGroupingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSmartTabGrouping)},
    {"persist-tab-context", flag_descriptions::kPersistTabContextName,
     flag_descriptions::kPersistTabContextDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kPersistTabContext,
                                    kPersistTabContextVariations,
                                    "PersistTabContext")},
    {"composebox-uses-chrome-compose-client",
     flag_descriptions::kNtpComposeboxUsesChromeComposeClientName,
     flag_descriptions::kNtpComposeboxUsesChromeComposeClientDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kComposeboxUsesChromeComposeClient)},
    {"composebox-verbatim-suggestion-in-aim",
     flag_descriptions::kComposeboxVerbatimSuggestionInAIMName,
     flag_descriptions::kComposeboxVerbatimSuggestionInAIMDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kComposeboxVerbatimSuggestionInAIM)},
    {"start-surface-user-setting",
     flag_descriptions::kStartSurfaceUserSettingName,
     flag_descriptions::kStartSurfaceUserSettingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kStartSurfaceUserSetting)},
    {"ios-tips-notifications-string-alternatives",
     flag_descriptions::kIOSTipsNotificationsStringAlternativesName,
     flag_descriptions::kIOSTipsNotificationsStringAlternativesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSTipsNotificationsAlternativeStrings)},
    {"variations-experimental-corpus",
     flag_descriptions::kVariationsExperimentalCorpusName,
     flag_descriptions::kVariationsExperimentalCorpusDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kVariationsExperimentalCorpus)},
    {"variations-restrict-dogfood",
     flag_descriptions::kVariationsRestrictDogfoodName,
     flag_descriptions::kVariationsRestrictDogfoodDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kVariationsRestrictDogfood)},
    {"zero-state-suggestions", flag_descriptions::kZeroStateSuggestionsName,
     flag_descriptions::kZeroStateSuggestionsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kZeroStateSuggestions)},
    {"zero-state-suggestions-wcgd",
     flag_descriptions::kZeroStateSuggestionsWCGDName,
     flag_descriptions::kZeroStateSuggestionsWCGDDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kZeroStateSuggestionsWCGD)},
    {"zero-state-suggestions-centralization",
     flag_descriptions::kZeroStateSuggestionsCentralizationName,
     flag_descriptions::kZeroStateSuggestionsCentralizationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kZeroStateSuggestionsCentralization)},
    {"disable-keyboard-accessory",
     flag_descriptions::kDisableKeyboardAccessoryName,
     flag_descriptions::kDisableKeyboardAccessoryDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDisableKeyboardAccessory,
                                    kDisableKeyboardAccessoryVariations,
                                    "DisableKeyboardAccessoryVariations")},
    {"ai-omnibox-ask-placeholder",
     flag_descriptions::kAIOmniboxAskPlaceholderName,
     flag_descriptions::kAIOmniboxAskPlaceholderDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAIOmniboxAskPlaceholder)},
    {"enable-fusebox-keyboard-accessory",
     flag_descriptions::kEnableFuseboxKeyboardAccessoryName,
     flag_descriptions::kEnableFuseboxKeyboardAccessoryDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kEnableFuseboxKeyboardAccessory,
         kEnableFuseboxKeyboardAccessoryVariations,
         "EnableFuseboxKeyboardAccessoryVariations")},
    {"mdm-errors-for-dasher-accounts-handling",
     flag_descriptions::kHandleMdmErrorsForDasherAccountsName,
     flag_descriptions::kHandleMdmErrorsForDasherAccountsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kHandleMdmErrorsForDasherAccounts)},
    {"location-bar-badge-migration",
     flag_descriptions::kLocationBarBadgeMigrationName,
     flag_descriptions::kLocationBarBadgeMigrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLocationBarBadgeMigration)},
    {"composebox-additional-advanced-tools",
     flag_descriptions::kComposeboxAdditionalAdvancedToolsName,
     flag_descriptions::kComposeboxAdditionalAdvancedToolsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kComposeboxAdditionalAdvancedTools)},
    {"composebox-close-button-top-align",
     flag_descriptions::kComposeboxCloseButtonTopAlignName,
     flag_descriptions::kComposeboxCloseButtonTopAlignDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kComposeboxCloseButtonTopAlign)},
    {"composebox-conditional-plus-button",
     flag_descriptions::kComposeboxConditionalPlusButtonName,
     flag_descriptions::kComposeboxConditionalPlusButtonDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kComposeboxConditionalPlusButton,
                                    kComposeboxConditionalPlusButtonVariations,
                                    "ComposeboxConditionalPlusButton")},
    {"composebox-aim-nudge", flag_descriptions::kComposeboxAIMNudgeName,
     flag_descriptions::kComposeboxAIMNudgeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kComposeboxAIMNudge)},
    {"composebox-aim-rich-apc-extraction",
     flag_descriptions::kComposeboxAimRichAPCExtractionName,
     flag_descriptions::kComposeboxAimRichAPCExtractionDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kComposeboxAimRichAPCExtraction)},
    {"gemini-live", flag_descriptions::kGeminiLiveName,
     flag_descriptions::kGeminiLiveDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kGeminiLive,
                                    kGeminiLiveVariations,
                                    "GeminiLive")},
    {"gemini-live-dormant-reasons",
     flag_descriptions::kGeminiLiveDormantReasonsName,
     flag_descriptions::kGeminiLiveDormantReasonsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiLiveDormantReasons)},

    {"omnibox-crash-fix-kill-switch",
     flag_descriptions::kOmniboxCrashFixKillSwitchName,
     flag_descriptions::kOmniboxCrashFixKillSwitchDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOmniboxCrashFixKillSwitch)},
    {"aim-eligibility-service-start-with-profile",
     flag_descriptions::kAIMEligibilityServiceStartWithProfileName,
     flag_descriptions::kAIMEligibilityServiceStartWithProfileDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAIMEligibilityServiceStartWithProfile)},
    {"lens-omnient-shader-v2-enabled",
     flag_descriptions::kLensOmnientShaderV2EnabledName,
     flag_descriptions::kLensOmnientShaderV2EnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensOmnientShaderV2Enabled)},
    {"lens-stream-service-web-channel-transport-enabled",
     flag_descriptions::kLensStreamServiceWebChannelTransportEnabledName,
     flag_descriptions::kLensStreamServiceWebChannelTransportEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLensStreamServiceWebChannelTransportEnabled)},
    {"aimntp-entrypoint-tablet", flag_descriptions::kAIMNTPEntrypointTabletName,
     flag_descriptions::kAIMNTPEntrypointTabletDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAIMNTPEntrypointTablet)},
    {"aim-eligibility-refresh-ntp-modules",
     flag_descriptions::kAIMEligibilityRefreshNTPModulesName,
     flag_descriptions::kAIMEligibilityRefreshNTPModulesDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAIMEligibilityRefreshNTPModules)},
    {"ios-web-context-menu-new-title",
     flag_descriptions::kIOSWebContextMenuNewTitleName,
     flag_descriptions::kIOSWebContextMenuNewTitleDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSWebContextMenuNewTitle)},
    {"composebox-attachments-typed-state",
     flag_descriptions::kComposeboxAttachmentsTypedStateName,
     flag_descriptions::kComposeboxAttachmentsTypedStateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kComposeboxAttachmentsTypedState)},
    {"autofill-enable-wallet-branding",
     flag_descriptions::kAutofillEnableWalletBrandingName,
     flag_descriptions::kAutofillEnableWalletBrandingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableWalletBranding)},
    {"assistant-container", flag_descriptions::kAssistantContainerName,
     flag_descriptions::kAssistantContainerDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kAssistantContainer,
                                    kAssistantContainerVariations,
                                    "AssistantContainer")},
    {"composebox-plus-button-bottom-sheet",
     flag_descriptions::kComposeboxPlusButtonBottomSheetName,
     flag_descriptions::kComposeboxPlusButtonBottomSheetDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kComposeboxPlusButtonBottomSheet)},
    {"composebox-plus-button-bottom-sheet-large-carousel",
     flag_descriptions::kComposeboxPlusButtonLargeCarouselName,
     flag_descriptions::kComposeboxPlusButtonLargeCarouselDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kComposeboxPlusButtonLargeCarousel)},
    {"chrome-next-ia", flag_descriptions::kChromeNextIaName,
     flag_descriptions::kChromeNextIaDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kChromeNextIa)},
    {"enable-new-startup-flow", flag_descriptions::kEnableNewStartupFlowName,
     flag_descriptions::kEnableNewStartupFlowDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableNewStartupFlow)},
    {"gemini-experimental-guided-onboarding",
     flag_descriptions::kGeminiExperimentalGuidedOnboardingName,
     flag_descriptions::kGeminiExperimentalGuidedOnboardingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kGeminiExperimentalGuidedOnboarding,
         kGeminiExperimentalGuidedOnboardingVariations,
         "GeminiExperimentalGuidedOnboarding")},
    {"gemini-fre-experiment", flag_descriptions::kGeminiFREExperimentName,
     flag_descriptions::kGeminiFREExperimentDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kGeminiFREExperiment,
                                    kGeminiFREExperimentVariations,
                                    "GeminiFREExperiment")},
    {"gemini-fre-refactor", flag_descriptions::kGeminiFRERefactorName,
     flag_descriptions::kGeminiFRERefactorDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiFRERefactor)},
    {"gemini-coordinator-teardown-fix",
     flag_descriptions::kGeminiCoordinatorTeardownFixName,
     flag_descriptions::kGeminiCoordinatorTeardownFixDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kGeminiCoordinatorTeardownFix)},
    {"enable-dynamic-route-fetching",
     enterprise_net::flag_descriptions::kEnableDynamicRouteFetchingName,
     enterprise_net::flag_descriptions::kEnableDynamicRouteFetchingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(enterprise_net::kEnableDynamicRouteFetching)},
    {"enable-enterprise-watermarking-ios",
     flag_descriptions::kEnableEnterpriseWatermarkingIOSName,
     flag_descriptions::kEnableEnterpriseWatermarkingIOSDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableEnterpriseWatermarkingIOS)},
    {"disable-composebox-from-aimntp",
     flag_descriptions::kDisableComposeboxFromAIMNTPName,
     flag_descriptions::kDisableComposeboxFromAIMNTPDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDisableComposeboxFromAIMNTP)},
    {"ios-save-to-drive-signed-out",
     flag_descriptions::kIOSSaveToDriveSignedOutName,
     flag_descriptions::kIOSSaveToDriveSignedOutDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSSaveToDriveSignedOut)},
    {"autofill-enable-bottom-sheet-scan-card-and-fill",
     flag_descriptions::kAutofillEnableBottomSheetScanCardAndFillName,
     flag_descriptions::kAutofillEnableBottomSheetScanCardAndFillDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableBottomSheetScanCardAndFill)},
    {"fs-no-broadcast-experiment",
     flag_descriptions::kSmoothScrollingUseDelegateName,
     flag_descriptions::kSmoothScrollingUseDelegateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kSmoothScrollingUseDelegate)},
    {"model-based-page-classification",
     flag_descriptions::kModelBasedPageClassificationName,
     flag_descriptions::kModelBasedPageClassificationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kModelBasedPageClassification,
                                    kModelBasedPageClassificationVariations,
                                    "ModelBasedPageClassification")},
    {"page-action-menu-icon", flag_descriptions::kPageActionMenuIconName,
     flag_descriptions::kPageActionMenuIconDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kPageActionMenuIcon,
                                    kPageActionMenuIconVariations,
                                    "PageActionMenuIcon")},
    {"ios-choose-from-drive-signed-out",
     flag_descriptions::kIOSChooseFromDriveSignedOutName,
     flag_descriptions::kIOSChooseFromDriveSignedOutDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSChooseFromDriveSignedOut)},
    {"ios-save-to-photos-signed-out",
     flag_descriptions::kIOSSaveToPhotosSignedOutName,
     flag_descriptions::kIOSSaveToPhotosSignedOutDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSSaveToPhotosSignedOut)},
    {"aim-cobrowse", flag_descriptions::kAimCobrowseName,
     flag_descriptions::kAimCobrowseDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAimCobrowse)},
    {"ios-date-to-calendar-signed-out",
     flag_descriptions::kIOSDateToCalendarSignedOutName,
     flag_descriptions::kIOSDateToCalendarSignedOutDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSDateToCalendarSignedOut)},
    {"gemini-actor", flag_descriptions::kGeminiActorName,
     flag_descriptions::kGeminiActorDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kGeminiActor,
                                    kGeminiActorVariations,
                                    "GeminiActor")},
    {"gemini-multi-tab-context", flag_descriptions::kGeminiMultiTabContextName,
     flag_descriptions::kGeminiMultiTabContextDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiMultiTabContext)},
    {"password-save-in-context-error-resolution",
     flag_descriptions::kPasswordSaveInContextErrorResolutionName,
     flag_descriptions::kPasswordSaveInContextErrorResolutionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kPasswordSaveInContextErrorResolution)},
    {"sync-themes-ios", flag_descriptions::kSyncThemesIosName,
     flag_descriptions::kSyncThemesIosDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncThemesIos)},
    {"sync-account-settings", flag_descriptions::kSyncAccountSettingsName,
     flag_descriptions::kSyncAccountSettingsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncAccountSettings)},
    {"sync-ai-threads", flag_descriptions::kSyncAIThreadsName,
     flag_descriptions::kSyncAIThreadsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncAIThread)},
    {"sync-contextual-task", flag_descriptions::kSyncContextualTaskName,
     flag_descriptions::kSyncContextualTaskDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncContextualTask)},
    {"sync-autofill-valuable", flag_descriptions::kSyncAutofillValuableName,
     flag_descriptions::kSyncAutofillValuableDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncAutofillValuable)},
    {"sync-autofill-valuable-metadata",
     flag_descriptions::kSyncAutofillValuableMetadataName,
     flag_descriptions::kSyncAutofillValuableMetadataDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncAutofillValuableMetadata)},
    {"sync-wallet-flight-reservations",
     flag_descriptions::kSyncWalletFlightReservationsName,
     flag_descriptions::kSyncWalletFlightReservationsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncWalletFlightReservations)},
    {"sync-wallet-vehicle-registrations",
     flag_descriptions::kSyncWalletVehicleRegistrationsName,
     flag_descriptions::kSyncWalletVehicleRegistrationsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(syncer::kSyncWalletVehicleRegistrations)},
    {"feedback-entry-points-require-can-submit-feedback-capability",
     flag_descriptions::
         kFeedbackEntryPointsRequireCanSubmitFeedbackCapabilityName,
     flag_descriptions::
         kFeedbackEntryPointsRequireCanSubmitFeedbackCapabilityDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         kFeedbackEntryPointsRequireCanSubmitFeedbackCapability)},
    {"disable-feedback-for-ineligible-users",
     flag_descriptions::kDisableFeedbackForIneligibleUsersName,
     flag_descriptions::kDisableFeedbackForIneligibleUsersDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDisableFeedbackForIneligibleUsers)},
    {"include-system-log-in-feedback",
     flag_descriptions::kIncludeSystemLogInFeedbackName,
     flag_descriptions::kIncludeSystemLogInFeedbackDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIncludeSystemLogInFeedback)},
    {"fullscreen-refactoring", flag_descriptions::kFullscreenRefactoringName,
     flag_descriptions::kFullscreenRefactoringDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFullscreenRefactoring)},
    {"fullscreen-eased-transitions",
     flag_descriptions::kFullscreenEasedTransitionsName,
     flag_descriptions::kFullscreenEasedTransitionsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kFullscreenEasedTransitions)},
    {"autofill-ai-always-show-private-ai-notice",
     flag_descriptions::kAutofillAiAlwaysShowPrivateAiNoticeName,
     flag_descriptions::kAutofillAiAlwaysShowPrivateAiNoticeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::debug::kAutofillAiAlwaysShowPrivateAiNotice)},
    {"autofill-ai-available-by-default",
     flag_descriptions::kAutofillAiAvailableByDefaultName,
     flag_descriptions::kAutofillAiAvailableByDefaultDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiAvailableByDefault)},
    {"autofill-ai-dedupe-entities",
     flag_descriptions::kAutofillAiDedupeEntitiesName,
     flag_descriptions::kAutofillAiDedupeEntitiesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiDedupeEntities)},
    {"autofill-ai-wallet-flight-reservation",
     flag_descriptions::kAutofillAiWalletFlightReservationName,
     flag_descriptions::kAutofillAiWalletFlightReservationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAiWalletFlightReservation)},
    {"autofill-ai-wallet-vehicle-registration",
     flag_descriptions::kAutofillAiWalletVehicleRegistrationName,
     flag_descriptions::kAutofillAiWalletVehicleRegistrationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAiWalletVehicleRegistration)},
    {"autofill-ai-wallet-private-passes",
     flag_descriptions::kAutofillAiWalletPrivatePassesName,
     flag_descriptions::kAutofillAiWalletPrivatePassesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiWalletPrivatePasses)},
    {"autofill-ai-wallet-private-passes-deep-link",
     flag_descriptions::kAutofillAiWalletPrivatePassesDeepLinkName,
     flag_descriptions::kAutofillAiWalletPrivatePassesDeepLinkDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAiWalletPrivatePassesDeepLink)},
    {"autofill-ai-reauth-required",
     flag_descriptions::kAutofillAiReauthRequiredName,
     flag_descriptions::kAutofillAiReauthRequiredDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiReauthRequired)},
    {"autofill-ai-no-filling-icons-experiment",
     flag_descriptions::kAutofillAiNoFillingIconsExperimentName,
     flag_descriptions::kAutofillAiNoFillingIconsExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillAiNoFillingIconsExperiment)},
    {"autofill-ai-valuables-iph",
     flag_descriptions::kAutofillAiValuablesIPHName,
     flag_descriptions::kAutofillAiValuablesIPHDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(feature_engagement::kIPHAutofillAiValuablesFeature)},
    {"autofill-ai-with-data-schema",
     flag_descriptions::kAutofillAiWithDataSchemaName,
     flag_descriptions::kAutofillAiWithDataSchemaDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAiWithDataSchema)},
    {"autofill-ambient-autofill",
     flag_descriptions::kAutofillAmbientAutofillName,
     flag_descriptions::kAutofillAmbientAutofillDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAmbientAutofill)},
    {"ios-tab-reminders", flag_descriptions::kIOSTabRemindersName,
     flag_descriptions::kIOSTabRemindersDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(send_tab_to_self::kIOSTabReminders)},
    {"gemini-unary-migration", flag_descriptions::kGeminiUnaryMigrationName,
     flag_descriptions::kGeminiUnaryMigrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiUnaryMigration)},
    {"gemini-binary-migration", flag_descriptions::kGeminiBinaryMigrationName,
     flag_descriptions::kGeminiBinaryMigrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiBinaryMigration)},
    {"ios-cobalt", flag_descriptions::kIOSCobaltName,
     flag_descriptions::kIOSCobaltDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIOSCobalt)},
    {"ask-about-this-page", flag_descriptions::kAskAboutThisPageName,
     flag_descriptions::kAskAboutThisPageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAskAboutThisPage)},
    {"mobile-promo-on-desktop-with-reminder-wave-1",
     flag_descriptions::kMobilePromoOnDesktopWave1Name,
     flag_descriptions::kMobilePromoOnDesktopWave1Description, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kMobilePromoOnDesktopWithReminderWave1,
                                    kMobilePromoOnDesktopWave1Variations,
                                    "MobilePromoOnDesktopWithReminderWave1")},
    {"aim-url-navigation-fetch-enabled",
     flag_descriptions::kAimUrlNavigationFetchEnabledName,
     flag_descriptions::kAimUrlNavigationFetchEnabledDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kAimUrlNavigationFetchEnabled)},
    {"page-tools-feature-unavailability",
     flag_descriptions::kPageToolsFeatureUnavailabilityName,
     flag_descriptions::kPageToolsFeatureUnavailabilityDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kPageToolsFeatureUnavailability)},
    {"page-context-pdf", flag_descriptions::kPageContextPdfName,
     flag_descriptions::kPageContextPdfDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kPageContextPdf)},
    {"autofill-enable-wallet-branding-v2",
     flag_descriptions::kAutofillEnableWalletBrandingV2Name,
     flag_descriptions::kAutofillEnableWalletBrandingV2Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableWalletBrandingV2)},
    {"autofill-enable-wallet-reminder-notice",
     flag_descriptions::kAutofillEnableWalletReminderNoticeName,
     flag_descriptions::kAutofillEnableWalletReminderNoticeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableWalletReminderNotice)},
    {"assistant-side-panel", flag_descriptions::kAssistantSidePanelName,
     flag_descriptions::kAssistantSidePanelDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAssistantSidePanel)},
    {"your-saved-info-settings-page-ios",
     flag_descriptions::kYourSavedInfoSettingsPageIosName,
     flag_descriptions::kYourSavedInfoSettingsPageIosDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kYourSavedInfoSettingsPageIos)},
    {"ios-actor-tools", flag_descriptions::kIOSActorToolsName,
     flag_descriptions::kIOSActorToolsDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kActorTools,
                                    kActorToolsPageStabilityVariations,
                                    "ActorTools")},
    {"ios-cobalt-developer-mode",
     flag_descriptions::kIOSCobaltDeveloperModeName,
     flag_descriptions::kIOSCobaltDeveloperModeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIOSCobaltDeveloperMode)},
    {"autofill-upstream-enforce-strike-delay",
     flag_descriptions::kAutofillUpstreamEnforceStrikeDelayName,
     flag_descriptions::kAutofillUpstreamEnforceStrikeDelayDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillUpstreamEnforceStrikeDelay)},
    {"no-accounts-web-signin", flag_descriptions::kNoAccountWebSigninName,
     flag_descriptions::kNoAccountWebSigninDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kNoAccountWebSignin)},
    {"assistant-aim-minimized-state",
     flag_descriptions::kAssistantAimMinimizedStateName,
     flag_descriptions::kAssistantAimMinimizedStateDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAssistantAimMinimizedState)},
    {"lens-followups-full-height-enabled",
     flag_descriptions::kLensFollowupsFullHeightEnabledName,
     flag_descriptions::kLensFollowupsFullHeightEnabledDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLensFollowupsFullHeightEnabled)},
    {"cross-device-signin", flag_descriptions::kCrossDeviceSigninName,
     flag_descriptions::kCrossDeviceSigninDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(switches::kCrossDeviceSignin,
                                    kCrossDeviceSigninVariations,
                                    "CrossDeviceSignin")},
    {"use-ui-graphics-image-renderer-for-fallback-icons",
     flag_descriptions::kUseUIGraphicsImageRendererForFallbackIconsName,
     flag_descriptions::kUseUIGraphicsImageRendererForFallbackIconsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseUIGraphicsImageRendererForFallbackIcons)},
    {"password-auto-submission-ios",
     flag_descriptions::kIOSPasswordAutoSubmissionName,
     flag_descriptions::kIOSPasswordAutoSubmissionDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         password_manager::features::kIOSPasswordAutoSubmission,
         kAutoSubmissionVariations,
         "IOSPasswordAutofillAutoSubmission")},
    {"ios-level-up", flag_descriptions::kIOSLevelUpName,
     flag_descriptions::kIOSLevelUpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSLevelUp)},
    {"ios-backend-promo-custom-ui",
     flag_descriptions::kIOSBackendPromoCustomUIName,
     flag_descriptions::kIOSBackendPromoCustomUIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSBackendPromoCustomUI)},
    {"ios-backend-promo-service-integration",
     flag_descriptions::kIOSBackendPromoServiceIntegrationName,
     flag_descriptions::kIOSBackendPromoServiceIntegrationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSBackendPromoServiceIntegration)},
    {"ios-background-metrics", flag_descriptions::kIOSBackgroundMetricsName,
     flag_descriptions::kIOSBackgroundMetricsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(metrics::features::kIOSBackgroundMetrics)},

    {"data-controls-search-with",
     flag_descriptions::kDataControlsSearchWithName,
     flag_descriptions::kDataControlsSearchWithDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(data_controls::kDataControlsSearchWith)},
    {"actor-service-logging", flag_descriptions::kActorServiceLoggingName,
     flag_descriptions::kActorServiceLoggingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kActorServiceLogging)},
    {"identity-awareness", flag_descriptions::kIdentityAwarenessName,
     flag_descriptions::kIdentityAwarenessDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIdentityAwareness)},
    {"autofill-enable-gradient-google-logos",
     flag_descriptions::kAutofillEnableGradientGoogleLogosName,
     flag_descriptions::kAutofillEnableGradientGoogleLogosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableGradientGoogleLogos)},
    {"autofill-ai-wallet-pass-branding-2026",
     flag_descriptions::kAutofillAiWalletPassBranding2026Name,
     flag_descriptions::kAutofillAiWalletPassBranding2026Description,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillAiWalletPassBranding2026,
         kAutofillAiWalletPassBranding2026Variations,
         "AutofillAiWalletPassBranding2026")},
    {"ios-mini-map-linkified-address",
     flag_descriptions::kIOSMiniMapLinkifiedAddressName,
     flag_descriptions::kIOSMiniMapLinkifiedAddressDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSMiniMapLinkifiedAddress)},
    {"composebox-drive-context-menu-option",
     flag_descriptions::kComposeboxDriveContextMenuOptionName,
     flag_descriptions::kComposeboxDriveContextMenuOptionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kComposeboxDriveContextMenuOption)},
    {"composebox-drive-context-menu-option-disclaimer",
     flag_descriptions::kComposeboxDriveContextMenuOptionDisclaimerName,
     flag_descriptions::kComposeboxDriveContextMenuOptionDisclaimerDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(omnibox::kComposeboxDriveContextMenuOptionDisclaimer)},
    {"ios-password-manager-actor-login",
     flag_descriptions::kPasswordManagerActorLoginName,
     flag_descriptions::kPasswordManagerActorLoginDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kActorLogin)},
    {"download-list-pagination-ios",
     flag_descriptions::kDownloadListPaginationName,
     flag_descriptions::kDownloadListPaginationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDownloadListPagination)},
    {"ai-subscription-avatar-ring-ios",
     flag_descriptions::kAiSubscriptionAvatarRingIosName,
     flag_descriptions::kAiSubscriptionAvatarRingIosDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAiSubscriptionAvatarRingIOS)},
    {"ai-subscription-avatar-followup-ios",
     flag_descriptions::kAiSubscriptionAvatarRingFollowupIosName,
     flag_descriptions::kAiSubscriptionAvatarRingFollowupIosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAiSubscriptionAvatarRingFollowupIOS)},
    {"gemini-contextual-suggestions-cues",
     flag_descriptions::kGeminiContextualSuggestionsCuesName,
     flag_descriptions::kGeminiContextualSuggestionsCuesDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         contextual_cueing::kGeminiContextualSuggestionsCues,
         kGeminiContextualSuggestionsCuesVariations,
         "GeminiContextualSuggestionsCues")},
    {"gemini-contextual-suggestions-cues-ui-mode",
     flag_descriptions::kGeminiContextualSuggestionsCuesUiModeName,
     flag_descriptions::kGeminiContextualSuggestionsCuesUiModeDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         contextual_cueing::kGeminiContextualSuggestionsCuesUiMode,
         kGeminiContextualSuggestionsCuesUiModeVariations,
         "GeminiContextualSuggestionsCuesUiMode")},
    {"page-classification", flag_descriptions::kPageClassificationName,
     flag_descriptions::kPageClassificationDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kPageClassification,
                                    kPageClassificationVariations,
                                    "PageClassification")},
    {"infobar-banner-revamp", flag_descriptions::kInfobarBannerRevampName,
     flag_descriptions::kInfobarBannerRevampDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kInfobarBannerRevamp)},
    {"overflow-menu-ntp-refactor",
     flag_descriptions::kOverflowMenuNTPRefactorName,
     flag_descriptions::kOverflowMenuNTPRefactorDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOverflowMenuNTPRefactor)},
    {"overflow-menu-home-customization-entrypoint",
     flag_descriptions::kOverflowMenuHomeCustomizationEntrypointName,
     flag_descriptions::kOverflowMenuHomeCustomizationEntrypointDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kOverflowMenuHomeCustomizationEntrypoint)},
    {"support-google-one-deep-link",
     flag_descriptions::kSupportGoogleOneDeepLinkName,
     flag_descriptions::kSupportGoogleOneDeepLinkDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSupportGoogleOneDeepLink)},
    {"enable-discover-background-refresh",
     flag_descriptions::kEnableDiscoverBackgroundRefreshName,
     flag_descriptions::kEnableDiscoverBackgroundRefreshDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableDiscoverBackgroundRefresh)},
    {"enable-bulk-data-entry-connector-ios",
     flag_descriptions::kEnableBulkDataEntryConnectorIOSName,
     flag_descriptions::kEnableBulkDataEntryConnectorIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         enterprise_connectors::kEnableBulkDataEntryConnectorIOS)},
    {"gaia-auth-fetcher-dont-send-sid-cookies",
     flag_descriptions::kGaiaAuthFetcherDontSendSIDCookiesName,
     flag_descriptions::kGaiaAuthFetcherDontSendSIDCookiesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         switches::kDontIncludeSIDUnsecureCookiesInGaiaAuthFetcher)},
    {"gemini-quizzes", flag_descriptions::kGeminiQuizzesName,
     flag_descriptions::kGeminiQuizzesDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kGeminiQuizzes,
                                    kGeminiQuizzesVariations,
                                    "GeminiQuizzes")},
    {"ios-passkey-conditional-login-with-shim",
     flag_descriptions::kIOSPasskeyConditionalLoginWithShimName,
     flag_descriptions::kIOSPasskeyConditionalLoginWithShimDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSPasskeyConditionalLoginWithShim)},
    {"ios-passkey-modal-login-with-shim",
     flag_descriptions::kIOSPasskeyModalLoginWithShimName,
     flag_descriptions::kIOSPasskeyModalLoginWithShimDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSPasskeyModalLoginWithShim)},
    {"default-bottom-omnibox-on-ios",
     flag_descriptions::kDefaultBottomOmniboxOnIOSName,
     flag_descriptions::kDefaultBottomOmniboxOnIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDefaultBottomOmniboxOnIOS)},
    {"atmemory-context-menu-entry-point",
     flag_descriptions::kAtMemoryContextMenuEntryPointName,
     flag_descriptions::kAtMemoryContextMenuEntryPointDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAtMemoryContextMenuEntryPoint)},
    {"glass-toolbar", flag_descriptions::kGlassToolbarName,
     flag_descriptions::kGlassToolbarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGlassToolbar)},
    {"composebox-connected-apps-setting",
     flag_descriptions::kComposeboxConnectedAppsSettingName,
     flag_descriptions::kComposeboxConnectedAppsSettingDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kComposeboxConnectedAppsSetting)},
    {"next-old-design", flag_descriptions::kNextOldDesignName,
     flag_descriptions::kNextOldDesignDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNextOldDesign)},
    {"new-tab-page-ephemeral-theme",
     flag_descriptions::kNewTabPageEphemeralThemeName,
     flag_descriptions::kNewTabPageEphemeralThemeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewTabPageEphemeralTheme)},
    {"new-tab-page-ui-cleanup", flag_descriptions::kNewTabPageUICleanupName,
     flag_descriptions::kNewTabPageUICleanupDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kNewTabPageUICleanup,
                                    kNewTabPageUICleanupVariations,
                                    "NewTabPageUICleanup")},
    {"web-frame-tree", flag_descriptions::kWebFrameTreeName,
     flag_descriptions::kWebFrameTreeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kWebFrameTree)},
    {"glic-actor-autofill", flag_descriptions::kGlicActorAutofillName,
     flag_descriptions::kGlicActorAutofillDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kGlicActorAutofill)},
    {"new-geolocation-permission-delegate",
     flag_descriptions::kNewGeolocationPermissionDelegateName,
     flag_descriptions::kNewGeolocationPermissionDelegateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kNewGeolocationPermissionDelegate)},
    {"universal-opt-out-settings",
     flag_descriptions::kUniversalOptOutSettingsName,
     flag_descriptions::kUniversalOptOutSettingsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(universal_optout::features::kUniversalOptOutSettings)},
    {"fetch-device-authorization-keys",
     flag_descriptions::kFetchDeviceAuthorizationKeysName,
     flag_descriptions::kFetchDeviceAuthorizationKeysDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(webauthn::features::kFetchDeviceAuthorizationKeys)},
    {"recover-tabs-of-last-closed-window",
     flag_descriptions::kRecoverTabsOfLastClosedWindowName,
     flag_descriptions::kRecoverTabsOfLastClosedWindowDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kRecoverTabsOfLastClosedWindow)},
    {"saas-usage-reporting", flag_descriptions::kSaasUsageReportingName,
     flag_descriptions::kSaasUsageReportingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(enterprise_reporting::kSaasUsageReporting)},
    {"aim-3p-entrypoint", flag_descriptions::kAim3pEntrypointName,
     flag_descriptions::kAim3pEntrypointDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(omnibox::kAim3pEntrypoint,
                                    kAim3pEntrypointVariations,
                                    "Aim3pEntrypoint")},
    {"aim-button-refactor", flag_descriptions::kAimButtonRefactorName,
     flag_descriptions::kAimButtonRefactorDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kAimButtonRefactor,
                                    kAimButtonRefactorVariations,
                                    "AimButtonRefactor")},
    {"enable-cobrowse-on-aim-srp-tap",
     flag_descriptions::kEnableCobrowseOnAimSrpTapName,
     flag_descriptions::kEnableCobrowseOnAimSrpTapDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableCobrowseOnAimSrpTap)},
    {"ios-signal-sharing", flag_descriptions::kIOSSignalSharingName,
     flag_descriptions::kIOSSignalSharingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(enterprise_reporting::kIOSSignalSharingEnabled)},
    {"metrickit-defer-registration",
     flag_descriptions::kMetrickitDeferRegistrationName,
     flag_descriptions::kMetrickitDeferRegistrationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kMetrickitDeferRegistration)},
    {"autofill-enable-wallet-disclosure-notice-public-pass",
     flag_descriptions::kAutofillEnableWalletDisclosureNoticePublicPassName,
     flag_descriptions::
         kAutofillEnableWalletDisclosureNoticePublicPassDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableWalletDisclosureNoticePublicPass)},
    {"autofill-enable-wallet-reminder-notice-public-pass",
     flag_descriptions::kAutofillEnableWalletReminderNoticePublicPassName,
     flag_descriptions::
         kAutofillEnableWalletReminderNoticePublicPassDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableWalletReminderNoticePublicPass)},
    {"gemini-insights-chip-ablation",
     flag_descriptions::kGeminiInsightsChipAblationName,
     flag_descriptions::kGeminiInsightsChipAblationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kGeminiInsightsChipAblation)},
    {"domain-level-site-permissions",
     flag_descriptions::kDomainLevelSitePermissionsName,
     flag_descriptions::kDomainLevelSitePermissionsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kDomainLevelSitePermissions)},
    {"enable-account-preview-preferred-account",
     flag_descriptions::kEnableAccountPreviewPreferredAccountName,
     flag_descriptions::kEnableAccountPreviewPreferredAccountDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kEnableAccountPreviewPreferredAccount)},
    {"enable-account-preview-preferred-account-followup",
     flag_descriptions::kEnableAccountPreviewPreferredAccountFollowupName,
     flag_descriptions::
         kEnableAccountPreviewPreferredAccountFollowupDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         switches::kEnableAccountPreviewPreferredAccountFollowup)},
    {"enable-account-preview-switching-account",
     flag_descriptions::kEnableAccountPreviewSwitchingAccountName,
     flag_descriptions::kEnableAccountPreviewSwitchingAccountDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         switches::kEnableAccountPreviewSwitchingAccount,
         kEnableAccountPreviewSwitchingAccountVariations,
         "EnableAccountPreviewSwitchingAccount")},
    {"aim-history-threads-management",
     flag_descriptions::kAimHistoryThreadsManagementName,
     flag_descriptions::kAimHistoryThreadsManagementDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAimHistoryThreadsManagement)},
    {"client-side-detection-ios",
     flag_descriptions::kClientSideDetectionIosName,
     flag_descriptions::kClientSideDetectionIosDescription, flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kClientSideDetectionIosChoices)},
    {"client-side-detection-server-model-for-scam-detection-ios",
     flag_descriptions::kClientSideDetectionServerModelForScamDetectionIosName,
     flag_descriptions::
         kClientSideDetectionServerModelForScamDetectionIosDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         safe_browsing::kClientSideDetectionServerModelForScamDetectionIos)},
    {"browsing-history-filter-by-device",
     flag_descriptions::kBrowsingHistoryFilterByDeviceName,
     flag_descriptions::kBrowsingHistoryFilterByDeviceDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(history::kBrowsingHistoryFilterByDevice)},
    {"ttc-enabled", flag_descriptions::kTTCEnabledName,
     flag_descriptions::kTTCEnabledDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTTCEnabled)},
    {"browsing-history-similar-visits-grouping",
     flag_descriptions::kBrowsingHistorySimilarVisitsGroupingName,
     flag_descriptions::kBrowsingHistorySimilarVisitsGroupingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(history::kBrowsingHistorySimilarVisitsGrouping)},
    {"universal-opt-out-extension",
     flag_descriptions::kUniversalOptOutExtensionName,
     flag_descriptions::kUniversalOptOutExtensionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(universal_optout::features::kUniversalOptOutExtension)},
    {"import-export-flags", flag_descriptions::kImportExportFlagsName,
     flag_descriptions::kImportExportFlagsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kImportExportFlags)},
    {"aim-cobrowse-web-selection-search",
     flag_descriptions::kAimCobrowseWebSelectionSearchName,
     flag_descriptions::kAimCobrowseWebSelectionSearchDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAimCobrowseWebSelectionSearch)},
    {"gemini-host-experiment-ids",
     flag_descriptions::kGeminiHostExperimentIDsName,
     flag_descriptions::kGeminiHostExperimentIDsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kGeminiHostExperimentIDs)},
    {"enable-segmentation-platform-ephemeral_card_ranker",
     flag_descriptions::kSegmentationPlatformEphemeralCardRankerName,
     flag_descriptions::kSegmentationPlatformEphemeralCardRankerDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         segmentation_platform::features::
             kSegmentationPlatformEphemeralCardRanker,
         kEphemeralCardRankerCardOverrideOptions,
         "SegmentationPlatformEphemeralCardRanker")},
    {"context-menu-preview-downsample-image",
     flag_descriptions::kContextMenuPreviewDownsampleImageName,
     flag_descriptions::kContextMenuPreviewDownsampleImageDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kContextMenuPreviewDownsampleImage)},
    {"ios-device-trust-connector",
     flag_descriptions::kIOSDeviceTrustConnectorName,
     flag_descriptions::kIOSDeviceTrustConnectorDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         enterprise_connectors::features::kEnableIOSDeviceTrustConnector)},
    {"enterprise-proxy-error-handling",
     enterprise_net::flag_descriptions::kEnterpriseProxyErrorHandlingName,
     enterprise_net::flag_descriptions::
         kEnterpriseProxyErrorHandlingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         enterprise_net::kEnterpriseProxyErrorHandling,
         kEnterpriseProxyErrorHandlingVariations,
         "EnterpriseProxyErrorHandling")},
    {"app-bar-assistant-customization",
     flag_descriptions::kAppBarAssistantCustomizationName,
     flag_descriptions::kAppBarAssistantCustomizationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAppBarAssistantCustomization)},
    {"light-app-bar", flag_descriptions::kLightAppBarName,
     flag_descriptions::kLightAppBarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kLightAppBar)},
    {"assistant-aim-threads", flag_descriptions::kAssistantAimThreadsName,
     flag_descriptions::kAssistantAimThreadsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAssistantAimThreads)},
    {"voice-search-evolution-phase1",
     flag_descriptions::kVoiceSearchEvolutionPhase1Name,
     flag_descriptions::kVoiceSearchEvolutionPhase1Description,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kVoiceSearchEvolutionPhase1)},
    {"autofill-enable-oauth2-token-fix",
     flag_descriptions::kAutofillEnableOAuth2TokenFixName,
     flag_descriptions::kAutofillEnableOAuth2TokenFixDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillEnableOAuth2TokenFix)},
    {"enable-account-preview-preferred-account-fre",
     flag_descriptions::kEnableAccountPreviewPreferredAccountFREName,
     flag_descriptions::kEnableAccountPreviewPreferredAccountFREDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kEnableAccountPreviewPreferredAccountFRE)},
    {"five-button-app-bar-layout",
     flag_descriptions::kFiveButtonAppBarLayoutName,
     flag_descriptions::kFiveButtonAppBarLayoutDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kFiveButtonAppBarLayout)},
    {"migrate-to-signin-reauth-coordinator",
     flag_descriptions::kMigrateToSigninReauthCoordinatorName,
     flag_descriptions::kMigrateToSigninReauthCoordinatorDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kMigrateToSigninReauthCoordinator)},
    {"ios-unified-manual-fallback-sheet",
     flag_descriptions::kIOSUnifiedManualFallbackSheetName,
     flag_descriptions::kIOSUnifiedManualFallbackSheetDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIOSUnifiedManualFallbackSheet)},
    {"location-bar-performance-improvements",
     flag_descriptions::kLocationBarPerformanceImprovementsName,
     flag_descriptions::kLocationBarPerformanceImprovementsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kLocationBarPerformanceImprovements)},
    {"app-bar-static-symbol-when-hidden",
     flag_descriptions::kAppBarStaticSymbolWhenHiddenName,
     flag_descriptions::kAppBarStaticSymbolWhenHiddenDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAppBarStaticSymbolWhenHidden)},
    {"app-bar-disable-long-press",
     flag_descriptions::kAppBarDisableLongPressName,
     flag_descriptions::kAppBarDisableLongPressDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAppBarDisableLongPress)},
    {"android-switcher-default-browser-promo-in-fre",
     flag_descriptions::kAndroidSwitcherDefaultBrowserPromoInFREName,
     flag_descriptions::kAndroidSwitcherDefaultBrowserPromoInFREDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         first_run::kAndroidSwitcherDefaultBrowserPromoInFRE,
         kAndroidSwitcherDefaultBrowserPromoInFREVariations,
         "AndroidSwitcherDefaultBrowserPromoInFRE")},
    {"magic-stack-ntp-theme-tip-ios",
     flag_descriptions::kMagicStackNTPThemeTipIosName,
     flag_descriptions::kMagicStackNTPThemeTipIosDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         segmentation_platform::features::kMagicStackNTPThemeTipIos,
         kMagicStackNTPThemeTipIosVariations,
         "MagicStackNTPThemeTipIos")},
    {"magic-stack-ntp-customization-tip-ios",
     flag_descriptions::kMagicStackNTPCustomizationTipIosName,
     flag_descriptions::kMagicStackNTPCustomizationTipIosDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         segmentation_platform::features::kMagicStackNTPCustomizationTipIos,
         kMagicStackNTPCustomizationTipIosVariations,
         "MagicStackNTPCustomizationTipIos")},
    {"magic-stack-tips-notification-tip-ios",
     flag_descriptions::kMagicStackTipsNotificationTipIosName,
     flag_descriptions::kMagicStackTipsNotificationTipIosDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         segmentation_platform::features::kMagicStackTipsNotificationTipIos,
         kMagicStackTipsNotificationTipIosVariations,
         "MagicStackTipsNotificationTipIos")},
    {"enable-background-continued-processing",
     flag_descriptions::kEnableBackgroundContinuedProcessingName,
     flag_descriptions::kEnableBackgroundContinuedProcessingDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableBackgroundContinuedProcessing)},
});

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
                           base::ListValue& supported_entries,
                           base::ListValue& unsupported_entries) {
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
  return kFeatureEntries;
}

}  // namespace testing
