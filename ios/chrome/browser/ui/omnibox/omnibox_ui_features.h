// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable omnibox suggestions scrolling on iPad. This will also
// disable suggestions hiding on keyboard dismissal.
BASE_DECLARE_FEATURE(kEnableSuggestionsScrollingOnIPad);

// Feature flag to enable omnibox suggestions with iOS 16 `PasteButton`.
BASE_DECLARE_FEATURE(kOmniboxPasteButton);

// Feature flag to make omnibox popup a floating rounded rect.
BASE_DECLARE_FEATURE(kEnablePopoutOmniboxIpad);

// Feature parameter for kOmniboxPasteButton.
extern const char kOmniboxPasteButtonParameterName[];
// PasteButton on suggestion row, blue color with icon only and capsule shape.
extern const char kOmniboxPasteButtonParameterBlueIconCapsule[];
// PasteButton on suggestion row, blue color with icon/text and capsule shape.
extern const char kOmniboxPasteButtonParameterBlueFullCapsule[];

// Feature flag to enable dynamic tile spacing in MVCarousel. Increases the
// spacing between the tiles to always show half a tile, indicating a scrollable
// list.
BASE_DECLARE_FEATURE(kOmniboxCarouselDynamicSpacing);

// Feature flag to enable paste button on the omnibox keyboard accessories.
BASE_DECLARE_FEATURE(kOmniboxKeyboardPasteButton);

// Simply returns if kIOSOmniboxUpdatedPopupUI is enabled.
bool IsOmniboxActionsEnabled();
// Returns true when kIOSOmniboxUpdatedPopupUI is set to "version 1" either in
// UIKit or SwiftUI.
bool IsOmniboxActionsVisualTreatment1();
// Same as above, but for "version 2".
bool IsOmniboxActionsVisualTreatment2();
// Returns false, swift version not supported anymore.
bool IsSwiftUIPopupEnabled();
// Returns if kEnablePopoutOmniboxIpad feature is enabled.
bool IsIpadPopoutOmniboxEnabled();
#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_
