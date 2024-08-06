// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TEST_FAKE_TAB_GRID_TOOLBARS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TEST_FAKE_TAB_GRID_TOOLBARS_MEDIATOR_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

@protocol GridToolbarsMutator;
@protocol TabGridToolbarsGridDelegate;
@protocol TabGridToolbarsConfiguration;

// Fake mediator class that implement the mutator to be able to verify the
// received value.
@interface FakeTabGridToolbarsMediator : NSObject <GridToolbarsMutator>

@property(nonatomic, strong) TabGridToolbarsConfiguration* configuration;
@property(nonatomic, weak) id<TabGridToolbarsGridDelegate> delegate;
@property(nonatomic, assign) BOOL enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TEST_FAKE_TAB_GRID_TOOLBARS_MEDIATOR_H_
