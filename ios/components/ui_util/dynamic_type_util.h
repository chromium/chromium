// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_UI_UTIL_DYNAMIC_TYPE_UTIL_H_
#define IOS_COMPONENTS_UI_UTIL_DYNAMIC_TYPE_UTIL_H_

#import <UIKit/UIKit.h>

namespace ui_util {

// Returns system suggested font size multiplier (e.g. 1.5 if the font size
// should be 50% bigger) for the actual system preferred content size category.
float SystemSuggestedFontSizeMultiplier();

// Returns system suggested font size multiplier (e.g. 1.5 if the font size
// should be 50% bigger) for the given `category`.
float SystemSuggestedFontSizeMultiplier(UIContentSizeCategory category);

// Returns system suggested font size multiplier (e.g. 1.5 if the font size
// should be 50% bigger) for the given `category`. The multiplier is clamped
// between the multipliers associated with `min_category` and `max_category`.
float SystemSuggestedFontSizeMultiplier(UIContentSizeCategory category,
                                        UIContentSizeCategory min_category,
                                        UIContentSizeCategory max_category);

}  // namespace ui_util

#endif  // IOS_COMPONENTS_UI_UTIL_DYNAMIC_TYPE_UTIL_H_
