// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"

#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

BASE_FEATURE(kExpandedTabStrip,
             "ExpandedTabStrip",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestFeature, "TestFeature", base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kDefaultBrowserRefactoringPromoManager,
             "DefaultBrowserRefactoringPromoManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserVideoPromo,
             "DefaultBrowserVideoPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSCustomBrowserEditMenu,
             "IOSCustomBrowserEditMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
    return base::FeatureList::IsEnabled(kIOSEditMenuSearchWith) &&
           base::FeatureList::IsEnabled(kIOSCustomBrowserEditMenu);
  }
  return false;
}

BASE_FEATURE(kIOSEditMenuHideSearchWeb,
             "IOSEditMenuHideSearchWeb",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSNewOmniboxImplementation,
             "kIOSNewOmniboxImplementation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSLocationBarUseNativeContextMenu,
             "IOSLocationBarUseNativeContextMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInHomeScreenWidget,
             "EnableLensInHomeScreenWidget",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInKeyboard,
             "EnableLensInKeyboard",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInNTP,
             "EnableLensInNTP",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensContextMenuAltText,
             "EnableLensContextMenuAltText",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensInOmniboxCopiedImage,
             "EnableLensInOmniboxCopiedImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kSFSymbolsFollowUp,
             "SFSymbolsFollowUp",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableExpKitAppleCalendar,
             "EnableExpKitAppleCalendar",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabGridRecencySort,
             "TabGridRecencySort",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabGridSortedByRecency() {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return false;
  }

  return base::FeatureList::IsEnabled(kTabGridRecencySort);
}

BASE_FEATURE(kTabGridNewTransitions,
             "TabGridNewTransitions",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsNewTabGridTransitionsEnabled() {
  return base::FeatureList::IsEnabled(kTabGridNewTransitions);
}

BASE_FEATURE(kMultilineFadeTruncatingLabel,
             "MultilineFadeTruncatingLabel",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kAddToHomeScreen,
             "AddToHomeScreen",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kAddToHomeScreenDisableIncognitoParam[] =
    "AddToHomeScreenDisableIncognitoParam";

bool ShouldAddToHomeScreen(bool in_incognito) {
  if (!base::FeatureList::IsEnabled(kAddToHomeScreen)) {
    return false;
  }
  if (!in_incognito) {
    return true;
  }
  return !base::GetFieldTrialParamByFeatureAsBool(
      kAddToHomeScreen, kAddToHomeScreenDisableIncognitoParam, true);
}

BASE_FEATURE(kNewNTPOmniboxLayout,
             "kNewNTPOmniboxLayout",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableEmailInBookmarksReadingListSnackbar,
             "EnableEmailInBookmarksReadingListSnackbar",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIndicateSyncErrorInOverflowMenu,
             "IndicateSyncErrorInOverflowMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsIndicateSyncErrorInOverflowMenuEnabled() {
  return base::FeatureList::IsEnabled(kIndicateSyncErrorInOverflowMenu);
}

BASE_FEATURE(kBottomOmniboxSteadyState,
             "BottomOmniboxSteadyState",
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
             base::FEATURE_DISABLED_BY_DEFAULT);
