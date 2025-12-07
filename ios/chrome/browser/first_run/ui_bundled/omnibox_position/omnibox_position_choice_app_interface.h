// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Test specific helpers for omnibox_position_egtest.mm.
@interface OmniboxPositionChoiceAppInterface : NSObject

// Shows the omnibox position choice screen.
+ (void)showOmniboxPositionChoiceScreen;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_APP_INTERFACE_H_
