// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_UI_BAR_BUTTON_ITEM_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_UI_BAR_BUTTON_ITEM_H_

#import <UIKit/UIKit.h>

@protocol OmniboxAssistiveKeyboardDelegate;

// UIBarButtonItem wrapper that calls OmniboxAssistiveKeyboardDelegate's
// `-keyPressed:` when pressed.
@interface OmniboxUIBarButtonItem : UIBarButtonItem

// Default initializer.
- (instancetype)initWithTitle:(NSString*)title
                     delegate:(id<OmniboxAssistiveKeyboardDelegate>)delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_UI_BAR_BUTTON_ITEM_H_
