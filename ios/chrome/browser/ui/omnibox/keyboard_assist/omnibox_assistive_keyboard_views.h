// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_VIEWS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_VIEWS_H_

#import <UIKit/UIKit.h>

@protocol OmniboxAssistiveKeyboardDelegate;

// Adds a keyboard assistive view [1] to |textField|. The assistive view
// contains among other things a button to quickly enter |dotComTLD|, and the
// callbacks are handled via |delegate|. |dotComTLD| must not be nil.
//
// [1]
// On iPhone the assistive view is a keyboard accessory view.
// On iPad, the assitive view is an inputAssistantItem to handle split
// keyboards.
void ConfigureAssistiveKeyboardViews(
    UITextField* textField,
    NSString* dotComTLD,
    id<OmniboxAssistiveKeyboardDelegate> delegate);

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_VIEWS_H_
