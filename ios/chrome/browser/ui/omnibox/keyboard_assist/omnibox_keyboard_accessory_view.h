// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_KEYBOARD_ACCESSORY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_KEYBOARD_ACCESSORY_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"

// Accessory View above the keyboard.
// Shows keys that are shortcuts to commonly used characters or strings,
// and buttons to start Voice Search or a Camera Search.
@interface OmniboxKeyboardAccessoryView : UIInputView <UIInputViewAudioFeedback>

// Designated initializer. |buttonTitles| lists the titles of the shortcut
// buttons. |delegate| receives the various events triggered in the view. Not
// retained, and can be nil.
- (instancetype)initWithButtons:(NSArray<NSString*>*)buttonTitles
                       delegate:(id<OmniboxAssistiveKeyboardDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithFrame:(CGRect)frame
               inputViewStyle:(UIInputViewStyle)inputViewStyle NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_KEYBOARD_ACCESSORY_VIEW_H_
