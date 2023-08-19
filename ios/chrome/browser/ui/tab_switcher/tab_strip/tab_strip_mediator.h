// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_consumer_delegate.h"

@protocol TabStripConsumer;
class WebStateList;
class ChromeBrowserState;

// This mediator used to manage model interaction for its consumer.
@interface TabStripMediator : NSObject <TabStripConsumerDelegate>

// The WebStateList that this mediator listens for any changes on the total
// number of Webstates.
@property(nonatomic, assign) WebStateList* webStateList;

// The ChromeBrowserState model for the corresponding browser.
@property(nonatomic, assign) ChromeBrowserState* browserState;

// Designated initializer. Initializer with a TabStripConsumer.
- (instancetype)initWithConsumer:(id<TabStripConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Preprares the receiver for destruction, disconnecting from all services.
// It is an error for the receiver to dealloc without this having been called
// first.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_MEDIATOR_H_
