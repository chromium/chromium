// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_mutator.h"

@protocol GridToolbarsMutator;
@protocol TabGridToolbarsMainTabGridDelegate;
class WebStateList;

// TabGroupsPanelMediator controls the Tab Groups panel in Tab Grid.
@interface TabGroupsPanelMediator : NSObject <TabGridPageMutator>

// WebStateList-s are used to configure the Done button.
// `regularWebStateList` must not be null.
// `disabled` tells the mediator whether the Tab Groups panel is disabled, to
// configure the toolbars.
- (instancetype)initWithRegularWebStateList:(WebStateList*)regularWebStateList
                           disabledByPolicy:(BOOL)disabled
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Mutator to handle toolbars modification.
@property(nonatomic, weak) id<GridToolbarsMutator> toolbarsMutator;

// Delegate handling the Tab Grid modifications.
@property(nonatomic, weak) id<TabGridToolbarsMainTabGridDelegate>
    toolbarTabGridDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MEDIATOR_H_
