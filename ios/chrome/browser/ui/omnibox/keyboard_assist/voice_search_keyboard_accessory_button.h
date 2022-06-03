// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_VOICE_SEARCH_KEYBOARD_ACCESSORY_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_VOICE_SEARCH_KEYBOARD_ACCESSORY_BUTTON_H_

#import <UIKit/UIKit.h>
#include <memory>

class VoiceSearchAvailability;

// A custom button that disables itself when voice search becomes unavailable.
@interface VoiceSearchKeyboardAccessoryButton : UIButton

- (instancetype)initWithVoiceSearchAvailability:
    (std::unique_ptr<VoiceSearchAvailability>)availability
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_VOICE_SEARCH_KEYBOARD_ACCESSORY_BUTTON_H_
