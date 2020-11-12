// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CONSUMER_H_

#import <Foundation/Foundation.h>

// TabStripConsumer sets the current appearance of the TabStrip.
@protocol TabStripConsumer

// Sets the number to tabs currently opened.
- (void)setTabsCount:(NSUInteger)tabsCount;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CONSUMER_H_
