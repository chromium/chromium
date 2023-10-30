// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_

#include "Availability.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Feature flag to enable default browser blue dot promo.
BASE_DECLARE_FEATURE(kDefaultBrowserBlueDotPromo);

// Feature flag to enable the Payments Bottom Sheet.
BASE_DECLARE_FEATURE(kIOSPaymentsBottomSheet);

// Enum for blue dot promo user groups (control/experiment) and its param. The
// reason why we need a custom control group is to disable other independent
// default browser promos, which are already shipped.
enum class BlueDotPromoUserGroup {
  kAllDBPromosDisabled,
  kAllDBPromosEnabled,
  kOnlyBlueDotPromoEnabled,
};
extern const base::FeatureParam<BlueDotPromoUserGroup>
    kBlueDotPromoUserGroupParam;

// Test-only: Feature flag used to verify that EG2 can trigger flags. Must be
// always disabled by default, because it is used to verify that enabling
// features in tests works.
BASE_DECLARE_FEATURE(kTestFeature);

// Feature to add the Safety Check module to the Magic Stack.
BASE_DECLARE_FEATURE(kSafetyCheckMagicStack);

// Feature flag to enable Shared Highlighting (Link to Text).
BASE_DECLARE_FEATURE(kSharedHighlightingIOS);

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished. Flag to modernize the tabstrip without disturbing the existing one.
BASE_DECLARE_FEATURE(kModernTabStrip);

// Feature flag to enable revamped Incognito NTP page.
BASE_DECLARE_FEATURE(kIncognitoNtpRevamp);

// Feature flag that allows external apps to show default browser settings.
BASE_DECLARE_FEATURE(kDefaultBrowserIntentsShowSettings);

// Feature flag to log metrics for the edit menu.
BASE_DECLARE_FEATURE(kIOSBrowserEditMenuMetrics);

// Feature flag to enable the non-modal DB promo cooldown refactor separating
// the cooldown periods for full screen and non-modal promos, as well as
// Finchable cooldown period for non-modal promos.
BASE_DECLARE_FEATURE(kNonModalDefaultBrowserPromoCooldownRefactor);

// The default param value for the non-modal promo cooldown period, in days,
// overridable through Finch.
extern const base::FeatureParam<int>
    kNonModalDefaultBrowserPromoCooldownRefactorParam;

// Feature flag to enable the default browser promo generic and tailored train
// experiment.
BASE_DECLARE_FEATURE(kDefaultBrowserGenericTailoredPromoTrain);

// Param values for the default browser promo generic and tailored train
// experiment.
enum class DefaultBrowserPromoGenericTailoredArm {
  kOnlyGeneric,
  kOnlyTailored,
};

// Feature param for the default browser promo generic and tailored train
// experiment.
extern const base::FeatureParam<DefaultBrowserPromoGenericTailoredArm>
    kDefaultBrowserPromoGenericTailoredParam;

// Feature flag that allows full screen default browser promos to be added to
// the promo manager.
BASE_DECLARE_FEATURE(kDefaultBrowserRefactoringPromoManager);

// Feature flag that enables the default browser video promo.
BASE_DECLARE_FEATURE(kDefaultBrowserVideoPromo);

// Feature param under kIOSEditMenuPartialTranslate to disable on incognito.
extern const char kIOSEditMenuPartialTranslateNoIncognitoParam[];
// Feature flag to enable partial translate in the edit menu.
BASE_DECLARE_FEATURE(kIOSEditMenuPartialTranslate);

// Helper function to check if kIOSEditMenuPartialTranslate is enabled and on
// supported OS.
bool IsPartialTranslateEnabled();

// Helper function to check if kIOSEditMenuPartialTranslate is enabled in
// incognito.
bool ShouldShowPartialTranslateInIncognito();

// Feature param under kIOSEditMenuSearchWith to select the title.
extern const char kIOSEditMenuSearchWithTitleParamTitle[];
extern const char kIOSEditMenuSearchWithTitleSearchParam[];
extern const char kIOSEditMenuSearchWithTitleSearchWithParam[];
extern const char kIOSEditMenuSearchWithTitleWebSearchParam[];
// Feature flag to enable search with in the edit menu.
BASE_DECLARE_FEATURE(kIOSEditMenuSearchWith);

// Helper function to check if kIOSEditMenuSearchWith is enabled and on
// supported OS.
bool IsSearchWithEnabled();

// Feature flag to hide search web in the edit menu.
BASE_DECLARE_FEATURE(kIOSEditMenuHideSearchWeb);

// Feature flag that swaps the omnibox textfield implementation.
BASE_DECLARE_FEATURE(kIOSNewOmniboxImplementation);

// Feature flag to use direct upload for Lens searches.
BASE_DECLARE_FEATURE(kIOSLensUseDirectUpload);

// Feature flag to enable the Lens entrypoint in the home screen widget.
BASE_DECLARE_FEATURE(kEnableLensInHomeScreenWidget);

// Feature flag to enable the Lens entrypoint in the keyboard.
BASE_DECLARE_FEATURE(kEnableLensInKeyboard);

// Feature flag to enable the Lens entrypoint in the new tab page.
BASE_DECLARE_FEATURE(kEnableLensInNTP);

// Feature flag to enable the Lens "Search copied image" omnibox entrypoint.
BASE_DECLARE_FEATURE(kEnableLensInOmniboxCopiedImage);

// Feature flag to enable UITraitCollection workaround for fixing incorrect
// trait propagation.
BASE_DECLARE_FEATURE(kEnableTraitCollectionWorkAround);

// Feature flag to enable the use of UIButtonConfigurations in iOS 15+.
BASE_DECLARE_FEATURE(kEnableUIButtonConfiguration);

// Returns true if the use of UIButtonConfigurations is enabled.
bool IsUIButtonConfigurationEnabled();

// Feature flag to enable duplicate NTP cleanup.
BASE_DECLARE_FEATURE(kRemoveExcessNTPs);

// Feature flag to enable shortened instruction to turn on Password AutoFill for
// Chrome.
BASE_DECLARE_FEATURE(kEnableShortenedPasswordAutoFillInstruction);

// Feature flag to enable Apple Calendar event in experience kit.
BASE_DECLARE_FEATURE(kEnableExpKitAppleCalendar);

// Feature flag / Kill Switch for TCRex.
BASE_DECLARE_FEATURE(kTCRexKillSwitch);

// When enabled uses new transitions in the TabGrid.
BASE_DECLARE_FEATURE(kTabGridNewTransitions);

// Whether the new tab grid tabs transitions should be enabled.
bool IsNewTabGridTransitionsEnabled();

// Feature flag to control the maximum amount of non-modal DB promo impressions
// server-side. Enabled by default to always have a default impression limit
// value.
BASE_DECLARE_FEATURE(kNonModalDefaultBrowserPromoImpressionLimit);

// The default param value for the non-modal DB promo impression limit,
// overridable through Finch. The associated histogram supports a maximum of 10
// impressions.
extern const base::FeatureParam<int>
    kNonModalDefaultBrowserPromoImpressionLimitParam;

// Flag to enable push notification settings menu item.
BASE_DECLARE_FEATURE(kNotificationSettingsMenuItem);

// Enables indexing Open tabs items in Spotlight.
BASE_DECLARE_FEATURE(kSpotlightOpenTabsSource);

// Enables indexing Reading List items in Spotlight.
BASE_DECLARE_FEATURE(kSpotlightReadingListSource);

// Enables intent donation for new intent types.
BASE_DECLARE_FEATURE(kSpotlightDonateNewIntents);

// Feature to enable sign-in only flow without device level account.
BASE_DECLARE_FEATURE(kConsistencyNewAccountInterface);

// Whether the flag for consistency new-account interface is enabled.
bool IsConsistencyNewAccountInterfaceEnabled();

// Feature flag to enable the new layout of the NTP omnibox.
BASE_DECLARE_FEATURE(kNewNTPOmniboxLayout);

// Feature flag to move the steady-state (unfocused) omnibox to the bottom.
BASE_DECLARE_FEATURE(kBottomOmniboxSteadyState);

// Feature param under kBottomOmniboxDefaultSetting to select the default
// setting.
extern const char kBottomOmniboxDefaultSettingParam[];
extern const char kBottomOmniboxDefaultSettingParamTop[];
extern const char kBottomOmniboxDefaultSettingParamBottom[];
extern const char kBottomOmniboxDefaultSettingParamSafariSwitcher[];
// Feature flag to change the default position of the omnibox.
BASE_DECLARE_FEATURE(kBottomOmniboxDefaultSetting);

// Feature flag to retrieve device switcher results for omnibox default
// position. Enabled by default.
BASE_DECLARE_FEATURE(kBottomOmniboxDeviceSwitcherResults);

// Returns true if `kBottomOmniboxSteadyState` feature flag is enabled and the
// current device is a phone. This checks that the flag is enabled, not that the
// omnibox is currently at the bottom.
bool IsBottomOmniboxSteadyStateEnabled();

// Returns true if `kBottomOmniboxDeviceSwitcherResults` feature flag is
// enabled.
bool IsBottomOmniboxDeviceSwitcherResultsEnabled();

// Feature flag to put all clipboard access onto a background thread. Any
// synchronous clipboard access will always return nil/false.
BASE_DECLARE_FEATURE(kOnlyAccessClipboardAsync);

// Feature flag that enables default browser video in settings experiment.
BASE_DECLARE_FEATURE(kDefaultBrowserVideoInSettings);

// Feature flag that enables default browser promo to be displayed without
// matching all the criteria and in depth metrics collection for the displayed
// promo.
BASE_DECLARE_FEATURE(kDefaultBrowserTriggerCriteriaExperiment);

// Feature flag to show default browser full-screen promo on omnbibox copy-paste
// event.
BASE_DECLARE_FEATURE(kFullScreenPromoOnOmniboxCopyPaste);

// Feature flag to try using the page theme color in the top toolbar
BASE_DECLARE_FEATURE(kThemeColorInTopToolbar);

// Feature flag to try using the page theme color as dynamic color for the
// toolbars.
BASE_DECLARE_FEATURE(kDynamicThemeColor);

// Feature flag to try using the page background color as dynamic color for the
// toolbars.
BASE_DECLARE_FEATURE(kDynamicBackgroundColor);

// Feature flag enabling tab grid refactoring.
BASE_DECLARE_FEATURE(kTabGridRefactoring);

// Whether the Safety Check module should be shown in the Magic Stack.
bool IsSafetyCheckMagicStackEnabled();

// Kill switch to control the blocking of the simultaneous cell selection in
// ChromeTableViewController.
BASE_DECLARE_FEATURE(kBlockSimultaneousCellSelectionKillSwitch);

// Feature flag enabling Save to Drive.
BASE_DECLARE_FEATURE(kIOSSaveToDrive);

// Feature flag enabling Save to Photos.
BASE_DECLARE_FEATURE(kIOSSaveToPhotos);

// Kill switch to control the `settingsWillBeDismissed` bug fix (see
// crbug.com/1482284).
BASE_DECLARE_FEATURE(kSettingsWillBeDismissedBugFixKillSwitch);

// Enables the new UIEditMenuInteraction system to be used in place of
// UIMenuController which was deprecated in iOS 16.
// TODO(crbug.com/1489734) Remove Flag once the minimum iOS deployment version
// has been increased to iOS 16.
BASE_DECLARE_FEATURE(kEnableUIEditMenuInteraction);

// Causes the restore shorty and re-signin flows to offer a history opt-in
// screen. This only has any effect if kReplaceSyncPromosWithSignInPromos is
// also enabled.
BASE_DECLARE_FEATURE(kHistoryOptInForRestoreShortyAndReSignin);

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_
