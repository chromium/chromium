// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_KEYBOARD_ASSIST_OMNIBOX_KEYBOARD_ACCESSORY_VIEW_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_KEYBOARD_ASSIST_OMNIBOX_KEYBOARD_ACCESSORY_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_delegate.h"

@protocol OmniboxTextInput;
class TemplateURLService;

// Accessory View above the keyboard.
// Shows keys that are shortcuts to commonly used characters or strings,
// and buttons to start Voice Search, Camera Search or Paste Search.
@interface OmniboxKeyboardAccessoryView : UIInputView <UIInputViewAudioFeedback>

// Designated initializer. `buttonTitles` lists the titles of the shortcut
// buttons. `delegate` receives the various events triggered in the view. Not
// retained, and can be nil.
- (instancetype)initWithButtons:(NSArray<NSString*>*)buttonTitles
                      showTools:(BOOL)showTools
                       delegate:(id<OmniboxAssistiveKeyboardDelegate>)delegate
                    pasteTarget:(id<UIPasteConfigurationSupporting>)pasteTarget
             templateURLService:(TemplateURLService*)templateURLService
                      responder:(UIResponder*)responder
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithFrame:(CGRect)frame
               inputViewStyle:(UIInputViewStyle)inputViewStyle NS_UNAVAILABLE;

// The templateURLService used by this view to determine whether or not
// Google is the default search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// Whether the keyboard accessory view should include tools like lens and voice
// search.
@property(nonatomic, assign, readonly) BOOL showTools;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_KEYBOARD_ASSIST_OMNIBOX_KEYBOARD_ACCESSORY_VIEW_H_
