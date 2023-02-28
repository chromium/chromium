// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer allowing the Browser View Controller to be updated when there is a
// change in WebStateList.
@protocol TabConsumer <NSObject>

// Tells the consumer to reset the content view.
- (void)resetTab;

// Tells the consumer to start an animation for a background tab.
- (void)animateNewBackgroundTab;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_CONSUMER_H_
