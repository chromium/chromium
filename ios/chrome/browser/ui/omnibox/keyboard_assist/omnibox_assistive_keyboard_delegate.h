// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class NamedGuide;
@class OmniboxTextFieldIOS;

// Delegate protocol for the KeyboardAccessoryView.
@protocol OmniboxAssistiveKeyboardDelegate

// Notifies the delegate that a touch up occurred in the Voice Search button.
- (void)keyboardAccessoryVoiceSearchTouchUpInside:(UIView*)view;

// Notifies the delegate that a touch up occurred in the Camera Search button.
- (void)keyboardAccessoryCameraSearchTouchUp;

// Notifies the delegate that a key with the title |title| was pressed.
- (void)keyPressed:(NSString*)title;

@end

// TODO(crbug.com/784819): Move this code to omnibox.
// Implementation of the OmniboxAssistiveKeyboardDelegate.
@interface OmniboxAssistiveKeyboardDelegateImpl
    : NSObject <OmniboxAssistiveKeyboardDelegate>

@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;
@property(nonatomic, weak) OmniboxTextFieldIOS* omniboxTextField;
@property(nonatomic, weak) NamedGuide* voiceSearchButtonGuide;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_DELEGATE_H_
