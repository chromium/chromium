// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_ADAPTOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_ADAPTOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_grid/tab_switcher.h"

@protocol TabGridPaging;
@class TabGridURLLoader;

// An opaque adaptor for the TabSwitcher protocol into the TabGrid.
// Consuming objects should be passed instances of this object as an
// id<TabSwitcher>.
// All of the methods and properties on this class are internal API fot the
// tab grid, and external code shouldn't depend on them.
@interface TabGridAdaptor : NSObject<TabSwitcher>
@property(nonatomic, weak) UIViewController* tabGridViewController;
// Dispatcher object this adaptor will expose as the dispacther for the
// TabSwitcher protocol.
@property(nonatomic, weak)
    id<ApplicationCommands, OmniboxFocuser, ToolbarCommands>
        adaptedDispatcher;
// Object that can set the current page of the tab grid.
@property(nonatomic, weak) id<TabGridPaging> tabGridPager;
// The mediator for the incognito grid.
@property(nonatomic, weak) TabGridMediator* incognitoMediator;
// Specialized URL loader for tab grid.
@property(nonatomic, weak) TabGridURLLoader* loader;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_ADAPTOR_H_
