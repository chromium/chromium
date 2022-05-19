// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_MEDIATOR_H_

#import <UIKit/UIKit.h>

class ChromeBrowserState;

// Mediator for the NTP Feed top section, handling the interactions.
@interface FeedTopSectionMediator : NSObject

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Initializes the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_MEDIATOR_H_
