// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_ROW_OMNIBOX_POPUP_ROW_UTIL_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_ROW_OMNIBOX_POPUP_ROW_UTIL_H_

#import <UIKit/UIKit.h>

/// Returns whether the omnibox popout layout should be applied.
BOOL ShouldApplyOmniboxPopoutLayout(UITraitCollection* traitCollection);

/// Returns the content size multiplier for the given category.
CGFloat OmniboxPopupRowContentSizeMultiplierForCategory(
    UIContentSizeCategory category);

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_ROW_OMNIBOX_POPUP_ROW_UTIL_H_
