// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_ADDRESS_BAR_PREFERENCE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_ADDRESS_BAR_PREFERENCE_CONSUMER_H_

#import <Foundation/Foundation.h>

// A protocol implemented by consumers to handle address bar preference state
// change.
@protocol AddressBarPreferenceConsumer

// Updates the state with the bottom omnibox preference value.
- (void)setPreferenceForOmniboxAtBottom:(BOOL)omniboxAtBottom;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_ADDRESS_BAR_PREFERENCE_CONSUMER_H_
