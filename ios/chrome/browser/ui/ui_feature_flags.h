// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_

#include "Availability.h"
#include "base/feature_list.h"

// Feature to open tab switcher after sliding down the toolbar.
extern const base::Feature kExpandedTabStrip;

// Test-only: Feature flag used to verify that EG2 can trigger flags. Must be
// always disabled by default, because it is used to verify that enabling
// features in tests works.
extern const base::Feature kTestFeature;

// Feature flag to enable Shared Highlighting (Link to Text).
extern const base::Feature kSharedHighlightingIOS;

// Feature flag for testing the 'default browser' screen in FRE and different
// experiments to suggest the users to update the default browser in the
// Settings.app.
extern const base::Feature kEnableFREDefaultBrowserPromoScreen;

// Feature flag that enables using the FRE UI module to show first run screens.
extern const base::Feature kEnableFREUIModuleIOS;

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished. Flag to modernize the tabstrip without disturbing the existing one.
extern const base::Feature kModernTabStrip;

// Feature flag to enable revamped Incognito NTP page.
extern const base::Feature kIncognitoNtpRevamp;

// Feature flag to enable third-party intents in Incognito.
extern const base::Feature kIOS3PIntentsInIncognito;

// Feature flag that experiments with the default browser fullscreen promo UI.
extern const base::Feature kDefaultBrowserFullscreenPromoExperiment;

// Feature flag that allows external apps to show default browser settings.
extern const base::Feature kDefaultBrowserIntentsShowSettings;

// Feature flag that shows iOS 15 context menu, instead of tooltip popover,
// during a location bar long press gesture.
extern const base::Feature kIOSLocationBarUseNativeContextMenu;

// Feature flag that swaps the omnibox textfield implementation.
extern const base::Feature kIOSNewOmniboxImplementation;

// Feature flag that toggles the SwiftUI omnibox popup implementation.
extern const base::Feature kIOSOmniboxUpdatedPopupUI;

// Parameter name for the parameter controlling which UI variation to use for
// the SwiftUI omnibox popup.
extern const char kIOSOmniboxUpdatedPopupUIVariationName[];

// Variation 1 for the parameter controlling which UI variation to use for
// the SwiftUI omnibox popup.
extern const char kIOSOmniboxUpdatedPopupUIVariation1[];
// Variation 2 for the parameter controlling which UI variation to use for
// the SwiftUI omnibox popup.
extern const char kIOSOmniboxUpdatedPopupUIVariation2[];
// Variation 3 for the parameter controlling the UI variation of the
// SwiftUI/UIKit pedals popup.
extern const char kIOSOmniboxUpdatedPopupUIVariation1UIKit[];
// Variation 4 for the parameter controlling the UI variation of the
// SwiftUI/UIKit pedals popup.
extern const char kIOSOmniboxUpdatedPopupUIVariation2UIKit[];

// Feature flag to enable removing any entry points to the history UI from
// Incognito mode.
extern const base::Feature kUpdateHistoryEntryPointsInIncognito;

// Feature flag to enable using Lens to search for images.
extern const base::Feature kUseLensToSearchForImage;

// Feature flag to enable duplicate NTP cleanup.
extern const base::Feature kRemoveExcessNTPs;

// Feature flag to enable shortened instruction to turn on Password AutoFill for
// Chrome.
extern const base::Feature kEnableShortenedPasswordAutoFillInstruction;

// Feature flag to show the setting allowing the user to choose the mode
// (Desktop/Mobile) in which the pages will be requested by default.
extern const base::Feature kAddSettingForDefaultPageMode;

// Feature flag to switch images to SFSymbols when enabled.
extern const base::Feature kUseSFSymbols;

// Feature flag to enable Calendar event in experience kit.
extern const base::Feature kCalendarExperienceKit;

// Feature flag to enable Apple Calendar event in experience kit.
extern const base::Feature kEnableExpKitAppleCalendar;

// Feature flag to enable Phone Numbers detection.
extern const base::Feature kEnablePhoneNumbers;

// Feature flag to enable Maps in experience kit.
extern const base::Feature kMapsExperienceKit;

// Feature flag to enable Mini Map in experience kit.
extern const base::Feature kEnableMiniMap;

#endif  // IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
