// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_MEDIATOR_H_

#import <Foundation/Foundation.h>

class Browser;
@protocol PinnedTabsCollectionConsumer;

// Mediates between model layer and pinned tabs collection UI layer.
@interface PinnedTabsMediator : NSObject

// The source browser.
@property(nonatomic, assign) Browser* browser;

// Initializer with `consumer` as the receiver of model layer updates.
- (instancetype)initWithConsumer:(id<PinnedTabsCollectionConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_MEDIATOR_H_
