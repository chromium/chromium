// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_ROW_OMNIBOX_POPUP_ROW_TRAILING_BUTTON_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_ROW_OMNIBOX_POPUP_ROW_TRAILING_BUTTON_H_

#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"

// The trailing icon type.
enum class TrailingIconType {
  /// No trailing icon.
  kNone,
  /// Append arrow icon type.
  kRefineQuery,
  /// Open existing tab icon type.
  kOpenExistingTab
};

// Trailing button view used in the omnibox popup row.
@interface OmniboxPopupRowTrailingButton : ExtendedTouchTargetButton

/// The trailing icon type.
@property(nonatomic, assign) TrailingIconType trailingIconType;

/// Whether or not the button row is highlighted.
@property(nonatomic, assign) BOOL isHighlighted;

/// The context in which the omnibox is presented.
@property(nonatomic, assign) OmniboxPresentationContext presentationContext;

/// Whether to point the arrow for refining a query downwards.
@property(nonatomic, assign) BOOL refineQueryArrowDirectionDown;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_ROW_OMNIBOX_POPUP_ROW_TRAILING_BUTTON_H_
