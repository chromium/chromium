// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_VIEWS_UTILS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_VIEWS_UTILS_H_

#import <UIKit/UIKit.h>

// The accessibility identifier for the Voice Search button.
extern NSString* const kVoiceSearchInputAccessoryViewID;

@protocol OmniboxAssistiveKeyboardDelegate;

// Returns the leading buttons of the assistive view.
NSArray<UIButton*>* OmniboxAssistiveKeyboardLeadingButtons(
    id<OmniboxAssistiveKeyboardDelegate> delegate);

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_VIEWS_UTILS_H_
