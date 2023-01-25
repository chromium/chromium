// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_

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

// Feature flag to enable third-party intents in Incognito.
BASE_DECLARE_FEATURE(kIOS3PIntentsInIncognito);

// Feature flag that experiments with the default browser fullscreen promo UI.
BASE_DECLARE_FEATURE(kDefaultBrowserFullscreenPromoExperiment);

// Feature flag that allows external apps to show default browser settings.
BASE_DECLARE_FEATURE(kDefaultBrowserIntentsShowSettings);

// Feature flag that shows iOS 15 context menu, instead of tooltip popover,
// during a location bar long press gesture.
BASE_DECLARE_FEATURE(kIOSLocationBarUseNativeContextMenu);

// Feature flag that swaps the omnibox textfield implementation.
BASE_DECLARE_FEATURE(kIOSNewOmniboxImplementation);

// Feature flag that removes the crash infobar.
BASE_DECLARE_FEATURE(kRemoveCrashInfobar);

// Feature flag to enable removing any entry points to the history UI from
// Incognito mode.
BASE_DECLARE_FEATURE(kUpdateHistoryEntryPointsInIncognito);

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

// Feature flag to enable duplicate NTP cleanup.
BASE_DECLARE_FEATURE(kRemoveExcessNTPs);

// Feature flag to enable shortened instruction to turn on Password AutoFill for
// Chrome.
BASE_DECLARE_FEATURE(kEnableShortenedPasswordAutoFillInstruction);

// Feature flag to switch images to SFSymbols when enabled.
BASE_DECLARE_FEATURE(kUseSFSymbols);

// Feature flag to switch images to SFSymbols in the omnibox when enabled.
BASE_DECLARE_FEATURE(kUseSFSymbolsInOmnibox);

// Feature flag to enable Calendar event in experience kit.
BASE_DECLARE_FEATURE(kCalendarExperienceKit);

// Feature flag to enable Apple Calendar event in experience kit.
BASE_DECLARE_FEATURE(kEnableExpKitAppleCalendar);

// Feature flag to enable Emails detection.
BASE_DECLARE_FEATURE(kEnableEmails);

// Feature flag to enable Phone Numbers detection.
BASE_DECLARE_FEATURE(kEnablePhoneNumbers);

// Parameter name for the parameter controlling whether or not experience kit
// maps should be enabled in search result pages or not.
extern const char kExperienceKitMapsVariationName[];

// Variation to enable experience kit Maps in search result pages.
extern const char kEnableExperienceKitMapsVariationSrp[];

// Feature flag to enable Maps in experience kit.
BASE_DECLARE_FEATURE(kMapsExperienceKit);

// Feature flag to enable Mini Map in experience kit.
BASE_DECLARE_FEATURE(kEnableMiniMap);

// When enabled sort tab by last usage in the TabGrid.
BASE_DECLARE_FEATURE(kTabGridRecencySort);

// Feature to enable multiline gradient support in fade truncating label.
BASE_DECLARE_FEATURE(kMultilineFadeTruncatingLabel);

// Flag to enable accessibility identifier to omnibox leading image.
BASE_DECLARE_FEATURE(kEnableAccessibilityIdentifierToOmniboxLeadingImage);

// Flag to enable tab strip context menu.
BASE_DECLARE_FEATURE(kTabStripContextMenu);

// Whether the tab grid tabs should be sorted by recency.
bool IsTabGridSortedByRecency();

#endif  // IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
