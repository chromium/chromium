// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_CONSUMER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_CONSUMER_H_

#import <UIKit/UIKit.h>

@class ComposeboxUIInputState;

// Consumer for the composebox menu.
@protocol ComposeboxMenuConsumer

// Sets the UI input state for the menu.
- (void)setUIInputState:(ComposeboxUIInputState*)state;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_CONSUMER_H_
