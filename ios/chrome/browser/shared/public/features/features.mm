// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"

#import "ui/base/device_form_factor.h"

BASE_FEATURE(kDefaultBrowserBlueDotPromo,
             "DefaultBrowserBlueDotPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<BlueDotPromoUserGroup>::Option
    kBlueDotPromoUserGroupOptions[] = {
        {BlueDotPromoUserGroup::kAllDBPromosDisabled, "all-db-promos-disabled"},
        {BlueDotPromoUserGroup::kAllDBPromosEnabled, "all-db-promos-enabled"},
        {BlueDotPromoUserGroup::kOnlyBlueDotPromoEnabled,
         "only-blue-dot-promo-enabled"}};

constexpr base::FeatureParam<BlueDotPromoUserGroup> kBlueDotPromoUserGroupParam{
    &kDefaultBrowserBlueDotPromo, "user-group",
    BlueDotPromoUserGroup::kAllDBPromosEnabled, &kBlueDotPromoUserGroupOptions};

BASE_FEATURE(kIOSPaymentsBottomSheet,
             "IOSPaymentsBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestFeature, "TestFeature", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyCheckMagicStack,
             "SafetyCheckMagicStack",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharedHighlightingIOS,
             "SharedHighlightingIOS",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished.
BASE_FEATURE(kModernTabStrip,
             "ModernTabStrip",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIncognitoNtpRevamp,
             "IncognitoNtpRevamp",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserIntentsShowSettings,
             "DefaultBrowserIntentsShowSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSBrowserEditMenuMetrics,
             "IOSBrowserEditMenuMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNonModalDefaultBrowserPromoCooldownRefactor,
             "NonModalDefaultBrowserPromoCooldownRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kNonModalDefaultBrowserPromoCooldownRefactorParam{
        &kNonModalDefaultBrowserPromoCooldownRefactor,
        /*name=*/"cooldown-days", /*default_value=*/14};

BASE_FEATURE(kDefaultBrowserGenericTailoredPromoTrain,
             "DefaultBrowserGenericTailoredPromoTrain",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<DefaultBrowserPromoGenericTailoredArm>::Option
    kDefaultBrowserPromoGenericTailoredArmOptions[] = {
        {DefaultBrowserPromoGenericTailoredArm::kOnlyGeneric, "only-generic"},
        {DefaultBrowserPromoGenericTailoredArm::kOnlyTailored,
         "only-tailored"}};

const base::FeatureParam<DefaultBrowserPromoGenericTailoredArm>
    kDefaultBrowserPromoGenericTailoredParam{
        &kDefaultBrowserGenericTailoredPromoTrain, "experiment-arm",
        DefaultBrowserPromoGenericTailoredArm::kOnlyGeneric,
        &kDefaultBrowserPromoGenericTailoredArmOptions};

BASE_FEATURE(kDefaultBrowserRefactoringPromoManager,
             "DefaultBrowserRefactoringPromoManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserVideoPromo,
             "DefaultBrowserVideoPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kIOSEditMenuPartialTranslateNoIncognitoParam[] =
    "IOSEditMenuPartialTranslateNoIncognitoParam";

BASE_FEATURE(kIOSEditMenuPartialTranslate,
             "IOSEditMenuPartialTranslate",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPartialTranslateEnabled() {
  if (@available(iOS 16, *)) {
    return base::FeatureList::IsEnabled(kIOSEditMenuPartialTranslate);
  }
  return false;
}

bool ShouldShowPartialTranslateInIncognito() {
  if (!IsPartialTranslateEnabled()) {
    return false;
  }
  return !base::GetFieldTrialParamByFeatureAsBool(
      kIOSEditMenuPartialTranslate,
      kIOSEditMenuPartialTranslateNoIncognitoParam, true);
}

const char kIOSEditMenuSearchWithTitleParamTitle[] =
    "IOSEditMenuSearchWithTitleParam";
const char kIOSEditMenuSearchWithTitleSearchParam[] = "Search";
const char kIOSEditMenuSearchWithTitleSearchWithParam[] = "SearchWith";
const char kIOSEditMenuSearchWithTitleWebSearchParam[] = "WebSearch";
BASE_FEATURE(kIOSEditMenuSearchWith,
             "IOSEditMenuSearchWith",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSearchWithEnabled() {
  if (@available(iOS 16, *)) {
    return base::FeatureList::IsEnabled(kIOSEditMenuSearchWith);
  }
  return false;
}

BASE_FEATURE(kIOSEditMenuHideSearchWeb,
             "IOSEditMenuHideSearchWeb",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSNewOmniboxImplementation,
             "kIOSNewOmniboxImplementation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSLensUseDirectUpload,
             "IOSLensUseDirectUpload",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInHomeScreenWidget,
             "EnableLensInHomeScreenWidget",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInKeyboard,
             "EnableLensInKeyboard",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInNTP,
             "EnableLensInNTP",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensContextMenuAltText,
             "EnableLensContextMenuAltText",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInOmniboxCopiedImage,
             "EnableLensInOmniboxCopiedImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableTraitCollectionWorkAround,
             "EnableTraitCollectionWorkAround",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableUIButtonConfiguration,
             "EnableUIButtonConfiguration",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUIButtonConfigurationEnabled() {
  return base::FeatureList::IsEnabled(kEnableUIButtonConfiguration);
}

BASE_FEATURE(kRemoveExcessNTPs,
             "RemoveExcessNTPs",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableShortenedPasswordAutoFillInstruction,
             "EnableShortenedPasswordAutoFillInstruction",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableExpKitAppleCalendar,
             "EnableExpKitAppleCalendar",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridNewTransitions,
             "TabGridNewTransitions",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewTabGridTransitionsEnabled() {
  return base::FeatureList::IsEnabled(kTabGridNewTransitions);
}

BASE_FEATURE(kMultilineFadeTruncatingLabel,
             "MultilineFadeTruncatingLabel",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNonModalDefaultBrowserPromoImpressionLimit,
             "NonModalDefaultBrowserPromoImpressionLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kNonModalDefaultBrowserPromoImpressionLimitParam{
        &kNonModalDefaultBrowserPromoImpressionLimit,
        /*name=*/"impression-limit", /*default_value=*/3};

BASE_FEATURE(kNotificationSettingsMenuItem,
             "NotificationSettingsMenuItem",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSpotlightOpenTabsSource,
             "SpotlightOpenTabsSource",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSpotlightReadingListSource,
             "SpotlightReadingListSource",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kConsistencyNewAccountInterface,
             "ConsistencyNewAccountInterface",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsConsistencyNewAccountInterfaceEnabled() {
  return base::FeatureList::IsEnabled(kConsistencyNewAccountInterface);
}

BASE_FEATURE(kNewNTPOmniboxLayout,
             "kNewNTPOmniboxLayout",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBottomOmniboxSteadyState,
             "BottomOmniboxSteadyState",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kBottomOmniboxDefaultSettingParam[] =
    "BottomOmniboxDefaultSettingParam";
const char kBottomOmniboxDefaultSettingParamTop[] = "Top";
const char kBottomOmniboxDefaultSettingParamBottom[] = "Bottom";
const char kBottomOmniboxDefaultSettingParamSafariSwitcher[] =
    "BottomSafariSwitcher";
BASE_FEATURE(kBottomOmniboxDefaultSetting,
             "BottomOmniboxDefaultSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBottomOmniboxSteadyStateEnabled() {
  // Bottom omnibox is only available on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return false;
  }
  return base::FeatureList::IsEnabled(kBottomOmniboxSteadyState);
}

BASE_FEATURE(kOnlyAccessClipboardAsync,
             "OnlyAccessClipboardAsync",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHideSettingsSyncPromo,
             "HideSettingsSyncPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserTriggerCriteriaExperiment,
             "DefaultBrowserTriggerCriteriaExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kDefaultBrowserTriggerOnOmniboxCopyPaste[] =
    "trigger_on_omnibox_copy_paste";

BASE_FEATURE(kThemeColorInToolbar,
             "ThemeColorInToolbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridRefactoring,
             "TabGridRefactoring",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridRefactoringFix,
             "TabGridRefactoringFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsSafetyCheckMagicStackEnabled() {
  return base::FeatureList::IsEnabled(kSafetyCheckMagicStack);
}

BASE_FEATURE(kSettingsWillBeDismissedBugFixKillSwitch,
             "SettingsWillBeDismissedBugFixKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);
