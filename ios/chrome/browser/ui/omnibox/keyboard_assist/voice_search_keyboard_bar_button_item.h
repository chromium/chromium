// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_VOICE_SEARCH_KEYBOARD_BAR_BUTTON_ITEM_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_VOICE_SEARCH_KEYBOARD_BAR_BUTTON_ITEM_H_

#import <UIKit/UIKit.h>
#include <memory>

class VoiceSearchAvailability;

// A custom bar button item that disables itself when voice search is
// unavailable.
@interface VoiceSearchKeyboardBarButtonItem : UIBarButtonItem

// Initializer for an item that disables itself when |availability| returns
// false for IsVoiceSearchAvailable().
- (instancetype)initWithImage:(UIImage*)image
                        style:(UIBarButtonItemStyle)style
                       target:(id)target
                       action:(SEL)action
      voiceSearchAvailability:
          (std::unique_ptr<VoiceSearchAvailability>)availability
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_VOICE_SEARCH_KEYBOARD_BAR_BUTTON_ITEM_H_
