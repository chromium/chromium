// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"

@class TabGridBottomToolbar;
@class TabGridTopToolbar;

// Mediates between model layer and top and bottom toolbar UI layer.
@interface TabGridToolbarsMediator : NSObject <GridToolbarsMutator>

// The toolbars consumer.
// TODO(crbug.com/1456659): Modify it to be consumers instead of being the full
// object.
@property(nonatomic, strong) TabGridTopToolbar* topToolbarConsumer;
@property(nonatomic, strong) TabGridBottomToolbar* bottomToolbarConsumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_MEDIATOR_H_
