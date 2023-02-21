// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_image_data_source.h"

class Browser;
@protocol TabCollectionConsumer;

@interface InactiveTabsMediator : NSObject <GridImageDataSource>

// The inactive browser reference.
@property(nonatomic, assign) Browser* inactiveBrowser;

// Initializer with `consumer` as the receiver of model layer updates.
- (instancetype)initWithConsumer:(id<TabCollectionConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_
