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
#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_
