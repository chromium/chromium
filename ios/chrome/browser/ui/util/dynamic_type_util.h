// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_DYNAMIC_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_DYNAMIC_TYPE_UTIL_H_

#import <UIKit/UIKit.h>

// Returns system suggested font size multiplier (e.g. 1.5 if the font size
// should be 50% bigger) for the actual system preferred content size category.
float SystemSuggestedFontSizeMultiplier();

// Returns system suggested font size multiplier (e.g. 1.5 if the font size
// should be 50% bigger) for the given |category|.
float SystemSuggestedFontSizeMultiplier(UIContentSizeCategory category);

// Returns system suggested font size multiplier (e.g. 1.5 if the font size
// should be 50% bigger) for the given |category|. The multiplier is clamped
// between the multipliers associated with |min_category| and |max_category|.
float SystemSuggestedFontSizeMultiplier(UIContentSizeCategory category,
                                        UIContentSizeCategory min_category,
                                        UIContentSizeCategory max_category);

// Returns an UIFont* calculated by |style| and
// min(|currentCategory|,|maxCategory|).
UIFont* PreferredFontForTextStyleWithMaxCategory(
    UIFontTextStyle style,
    UIContentSizeCategory currentCategory,
    UIContentSizeCategory maxCategory);

#endif  // IOS_CHROME_BROWSER_UI_UTIL_DYNAMIC_TYPE_UTIL_H_
