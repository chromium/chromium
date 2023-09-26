// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_VIEWS_UTILS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_VIEWS_UTILS_H_

#import <UIKit/UIKit.h>

// The accessibility identifier for the Voice Search button.
extern NSString* const kVoiceSearchInputAccessoryViewID;
// Height and width for the paste button.
extern CGFloat const kPasteButtonSize;

@protocol OmniboxAssistiveKeyboardDelegate;

// Updates the appearance of the Lens button after the ui interface environment
// changes.
void UpdateLensButtonAppearance(UIButton* button);

// Returns the leading buttons of the assistive view.
NSArray<UIControl*>* OmniboxAssistiveKeyboardLeadingControls(
    id<OmniboxAssistiveKeyboardDelegate> delegate,
    id<UIPasteConfigurationSupporting> pasteTarget,
    bool useLens);

#if defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
UIPasteControl* OmniboxAssistiveKeyboardPasteControl(
    id<UIPasteConfigurationSupporting> pasteTarget) API_AVAILABLE(ios(16));
#endif  // defined(__IPHONE_16_0)

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_VIEWS_UTILS_H_
