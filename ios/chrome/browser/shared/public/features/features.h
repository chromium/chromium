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

// Feature to open tab switcher after sliding down the toolbar.
BASE_DECLARE_FEATURE(kExpandedTabStrip);

// Test-only: Feature flag used to verify that EG2 can trigger flags. Must be
// always disabled by default, because it is used to verify that enabling
// features in tests works.
BASE_DECLARE_FEATURE(kTestFeature);

// Feature flag to enable Shared Highlighting (Link to Text).
BASE_DECLARE_FEATURE(kSharedHighlightingIOS);

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished. Flag to modernize the tabstrip without disturbing the existing one.
BASE_DECLARE_FEATURE(kModernTabStrip);

// Feature flag to enable revamped Incognito NTP page.
BASE_DECLARE_FEATURE(kIncognitoNtpRevamp);

// Feature flag that experiments with the default browser fullscreen promo UI.
BASE_DECLARE_FEATURE(kDefaultBrowserFullscreenPromoExperiment);

// Feature flag that allows external apps to show default browser settings.
BASE_DECLARE_FEATURE(kDefaultBrowserIntentsShowSettings);

// Feature flag to log metrics for the edit menu.
BASE_DECLARE_FEATURE(kIOSBrowserEditMenuMetrics);

// Feature flag that allows full screen default browser promos to be added to
// the promo manager.
BASE_DECLARE_FEATURE(kDefaultBrowserRefactoringPromoManager);

// Feature flag that enables the default browser video promo.
BASE_DECLARE_FEATURE(kDefaultBrowserVideoPromo);

// Feature flag to use the new Edit menu API for browser view.
BASE_DECLARE_FEATURE(kIOSCustomBrowserEditMenu);

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

// Feature flag that shows iOS 15 context menu, instead of tooltip popover,
// during a location bar long press gesture.
BASE_DECLARE_FEATURE(kIOSLocationBarUseNativeContextMenu);

// Feature flag that swaps the omnibox textfield implementation.
BASE_DECLARE_FEATURE(kIOSNewOmniboxImplementation);

// Feature flag to enable using Lens to search for images.
BASE_DECLARE_FEATURE(kUseLensToSearchForImage);

// Feature flag to enable the Lens entrypoint in the home screen widget.
BASE_DECLARE_FEATURE(kEnableLensInHomeScreenWidget);

// Feature flag to enable the Lens entrypoint in the keyboard.
BASE_DECLARE_FEATURE(kEnableLensInKeyboard);

// Feature flag to enable the Lens entrypoint in the new tab page.
BASE_DECLARE_FEATURE(kEnableLensInNTP);

// Feature flag to enable the Lens context menu alternate text string.
BASE_DECLARE_FEATURE(kEnableLensContextMenuAltText);

// Feature flag to enable the Lens "Search copied image" omnibox entrypoint.
BASE_DECLARE_FEATURE(kEnableLensInOmniboxCopiedImage);

// Feature flag to enable the use of UIButtonConfigurations in iOS 15+.
BASE_DECLARE_FEATURE(kEnableUIButtonConfiguration);

// Returns true if the use of UIButtonConfigurations is enabled.
bool IsUIButtonConfigurationEnabled();

// Feature flag to enable duplicate NTP cleanup.
BASE_DECLARE_FEATURE(kRemoveExcessNTPs);

// Feature flag to enable shortened instruction to turn on Password AutoFill for
// Chrome.
BASE_DECLARE_FEATURE(kEnableShortenedPasswordAutoFillInstruction);

// Feature flag to switch images to SFSymbols in the omnibox when enabled.
BASE_DECLARE_FEATURE(kUseSFSymbolsInOmnibox);

// Feature flag for the follow up of the SF Symbols.
BASE_DECLARE_FEATURE(kSFSymbolsFollowUp);

// Feature flag to enable Apple Calendar event in experience kit.
BASE_DECLARE_FEATURE(kEnableExpKitAppleCalendar);

// When enabled sort tab by last usage in the TabGrid.
BASE_DECLARE_FEATURE(kTabGridRecencySort);

// Whether the tab grid tabs should be sorted by recency.
bool IsTabGridSortedByRecency();

// When enabled uses new transitions in the TabGrid.
BASE_DECLARE_FEATURE(kTabGridNewTransitions);

// Whether the new tab grid tabs transitions should be enabled.
bool IsNewTabGridTransitionsEnabled();

// Feature to enable multiline gradient support in fade truncating label.
BASE_DECLARE_FEATURE(kMultilineFadeTruncatingLabel);

// Flag to enable push notification settings menu item.
BASE_DECLARE_FEATURE(kNotificationSettingsMenuItem);

// Enables indexing Reading List items in Spotlight.
BASE_DECLARE_FEATURE(kSpotlightReadingListSource);

// Feature to enable sign-in only flow without device level account.
BASE_DECLARE_FEATURE(kConsistencyNewAccountInterface);

// Whether the flag for consistency new-account interface is enabled.
bool IsConsistencyNewAccountInterfaceEnabled();

// Feature flag to enable add to home screen in share menu.
BASE_DECLARE_FEATURE(kAddToHomeScreen);

// Param to disable the feature in incognito.
extern const char kAddToHomeScreenDisableIncognitoParam[];

// Helper function to check the feature add to home screen.
bool ShouldAddToHomeScreen(bool in_incognito);

// Feature flag to enable the new layout of the NTP omnibox.
BASE_DECLARE_FEATURE(kNewNTPOmniboxLayout);

// Whether the email is shown in the snackbar indicating that a new bookmark
// or reading list item is added.
BASE_DECLARE_FEATURE(kEnableEmailInBookmarksReadingListSnackbar);

// Feature flag to enable indicating Sync errors (including identity errors)
// on the Settings destination in the overflow menu carousel.
BASE_DECLARE_FEATURE(kIndicateSyncErrorInOverflowMenu);

// Returns true if the `kIndicateSyncErrorInOverflowMenu` feature is enabled.
bool IsIndicateSyncErrorInOverflowMenuEnabled();

// Feature flag to move the steady-state (unfocused) omnibox to the bottom.
BASE_DECLARE_FEATURE(kBottomOmniboxSteadyState);

// Feature flag to put all clipboard access onto a background thread. Any
// synchronous clipboard access will always return nil/false.
BASE_DECLARE_FEATURE(kOnlyAccessClipboardAsync);

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_
