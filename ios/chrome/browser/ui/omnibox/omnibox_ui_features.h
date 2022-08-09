// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable omnibox suggestions scrolling on iPad. This will also
// disable suggestions hiding on keyboard dismissal.
extern const base::Feature kEnableSuggestionsScrollingOnIPad;

// Feature flag to enable omnibox suggestions with iOS 16 `PasteButton`.
extern const base::Feature kOmniboxPasteButton;

// Feature parameter for kOmniboxPasteButton.
extern const char kOmniboxPasteButtonParameterName[];
// PasteButton on suggestion row, blue color with icon only and capsule shape.
extern const char kOmniboxPasteButtonParameterBlueIconCapsule[];
// PasteButton on suggestion row, blue color with icon/text and capsule shape.
extern const char kOmniboxPasteButtonParameterBlueFullCapsule[];

// Feature flag to enable paste button on the omnibox keyboard accessories.
extern const base::Feature kOmniboxKeyboardPasteButton;

// Simply returns if kIOSOmniboxUpdatedPopupUI is enabled.
bool IsOmniboxActionsEnabled();
// Returns true when kIOSOmniboxUpdatedPopupUI is set to "version 1" either in
// UIKit or SwiftUI.
bool IsOmniboxActionsVisualTreatment1();
// Same as above, but for "version 2".
bool IsOmniboxActionsVisualTreatment2();
// Returns true when Actions are set to one of the SwiftUI variations.
bool IsSwiftUIPopupEnabled();

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_
