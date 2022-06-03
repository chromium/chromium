// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_INPUT_ASSISTANT_ITEMS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_INPUT_ASSISTANT_ITEMS_H_

#import <UIKit/UIKit.h>

@protocol OmniboxAssistiveKeyboardDelegate;

// Returns the leading button groups for the omnibox's inputAssistantItem.
NSArray<UIBarButtonItemGroup*>* OmniboxAssistiveKeyboardLeadingBarButtonGroups(
    id<OmniboxAssistiveKeyboardDelegate> delegate);

// Returns the trailing button groups for the omnibox's inputAssistantItem.
NSArray<UIBarButtonItemGroup*>* OmniboxAssistiveKeyboardTrailingBarButtonGroups(
    id<OmniboxAssistiveKeyboardDelegate> delegate,
    NSArray<NSString*>* buttonTitles);

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_INPUT_ASSISTANT_ITEMS_H_
