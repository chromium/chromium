// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable omnibox suggestions scrolling on iPad. This will also
// disable suggestions hiding on keyboard dismissal.
BASE_DECLARE_FEATURE(kEnableSuggestionsScrollingOnIPad);

// Feature flag to make omnibox popup a floating rounded rect.
BASE_DECLARE_FEATURE(kEnablePopoutOmniboxIpad);

// Feature flag to enable paste button on the omnibox keyboard accessories.
BASE_DECLARE_FEATURE(kOmniboxKeyboardPasteButton);

// Feature flag to enable multiple lines for search suggestions in omnibox.
BASE_DECLARE_FEATURE(kOmniboxMultilineSearchSuggest);

// Feature flag to enable tail suggestions in the omnibox.
BASE_DECLARE_FEATURE(kOmniboxTailSuggest);

// Returns if kEnablePopoutOmniboxIpad feature is enabled.
bool IsIpadPopoutOmniboxEnabled();

// Feature flag to enable improved RTL layout of the suggestions.
BASE_DECLARE_FEATURE(kOmniboxSuggestionsRTLImprovements);

// Kill switch to revert the removal of lock icon. When this feature is
// enabled, the lock icon is shown in the omnibox for secure pages. When
// disabled, no icon is shown for secure pages.
BASE_DECLARE_FEATURE(kOmniboxLockIconEnabled);

// Feature flag to enable storing successful query/match in the shortcut
// database.
BASE_DECLARE_FEATURE(kOmniboxPopulateShortcutsDatabase);

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_
