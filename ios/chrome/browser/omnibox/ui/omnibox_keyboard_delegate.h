// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_KEYBOARD_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_KEYBOARD_DELEGATE_H_

#import <UIKit/UIKit.h>

/// Keyboard actions used by both the textfield and popup.
enum class OmniboxKeyboardAction {
  kUpArrow,
  kDownArrow,
  kLeftArrow,
  kRightArrow,
  kReturnKey,
};

// Keyboard inputs in the omnibox are received by OmniboxTextFieldIOS. Some keys
// are handled by the textfield and others by the popup to control
// highlight/focus. These keys are forwarded to the popup. If the popup cannot
// handle these keys, it fallbacks to the text field. ex: Left and Right arrow
// keys can control the highlight of OmniboxPopupCarouselCell or move the text
// caret in OmniboxTextFieldIOS.
// Note: This API may be deprecated with iOS 14, with focus/highlight handled
// by UIFocusSystem available in iOS 15.
@protocol OmniboxKeyboardDelegate <NSObject>

/// Whether the `keyboardAction` can be performed.
- (BOOL)canPerformKeyboardAction:(OmniboxKeyboardAction)keyboardAction;

/// Performs the `keyboardAction`.
- (void)performKeyboardAction:(OmniboxKeyboardAction)keyboardAction;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_KEYBOARD_DELEGATE_H_
