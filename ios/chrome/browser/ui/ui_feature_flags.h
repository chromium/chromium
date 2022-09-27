// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_

#include "Availability.h"
#include "base/feature_list.h"

// Feature to open tab switcher after sliding down the toolbar.
BASE_DECLARE_FEATURE(kExpandedTabStrip);

// Test-only: Feature flag used to verify that EG2 can trigger flags. Must be
// always disabled by default, because it is used to verify that enabling
// features in tests works.
BASE_DECLARE_FEATURE(kTestFeature);

// Feature flag to enable Shared Highlighting (Link to Text).
BASE_DECLARE_FEATURE(kSharedHighlightingIOS);

// Feature flag for testing the 'default browser' screen in FRE and different
// experiments to suggest the users to update the default browser in the
// Settings.app.
BASE_DECLARE_FEATURE(kEnableFREDefaultBrowserPromoScreen);

// Feature flag that enables using the FRE UI module to show first run screens.
BASE_DECLARE_FEATURE(kEnableFREUIModuleIOS);

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

// Feature flag that toggles the SwiftUI omnibox popup implementation.
BASE_DECLARE_FEATURE(kIOSOmniboxUpdatedPopupUI);

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
BASE_DECLARE_FEATURE(kUpdateHistoryEntryPointsInIncognito);

// Feature flag to enable using Lens to search for images.
BASE_DECLARE_FEATURE(kUseLensToSearchForImage);

// Feature flag to enable the Lens entrypoint in the home screen widget.
BASE_DECLARE_FEATURE(kEnableLensInHomeScreenWidget);

// Feature flag to enable the Lens entrypoint in the keyboard.
BASE_DECLARE_FEATURE(kEnableLensInKeyboard);

// Feature flag to enable the Lens entrypoint in the new tab page.
BASE_DECLARE_FEATURE(kEnableLensInNTP);

// Feature flag to enable duplicate NTP cleanup.
BASE_DECLARE_FEATURE(kRemoveExcessNTPs);

// Feature flag to enable shortened instruction to turn on Password AutoFill for
// Chrome.
BASE_DECLARE_FEATURE(kEnableShortenedPasswordAutoFillInstruction);

// Feature flag to switch images to SFSymbols when enabled.
BASE_DECLARE_FEATURE(kUseSFSymbols);

// Feature flag to enable Calendar event in experience kit.
BASE_DECLARE_FEATURE(kCalendarExperienceKit);

// Feature flag to enable Apple Calendar event in experience kit.
BASE_DECLARE_FEATURE(kEnableExpKitAppleCalendar);

// Feature flag to enable Phone Numbers detection.
BASE_DECLARE_FEATURE(kEnablePhoneNumbers);

// Feature flag to enable Maps in experience kit.
BASE_DECLARE_FEATURE(kMapsExperienceKit);

// Feature flag to enable Mini Map in experience kit.
BASE_DECLARE_FEATURE(kEnableMiniMap);

#endif  // IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
