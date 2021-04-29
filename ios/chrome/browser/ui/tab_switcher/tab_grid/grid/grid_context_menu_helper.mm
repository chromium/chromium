// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_context_menu_helper.h"

#include "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/menu/tab_context_menu_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_menu_actions_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GridContextMenuHelper () <GridContextMenuProvider>

@property(nonatomic, assign) Browser* browser;

@property(nonatomic, weak) id<TabContextMenuDelegate> contextMenuDelegate;
@property(nonatomic, weak) id<GridMenuActionsDataSource> actionsDataSource;

@end

@implementation GridContextMenuHelper

#pragma mark - GridContextMenuProvider

- (instancetype)initWithBrowser:(Browser*)browser
              actionsDataSource:(id<GridMenuActionsDataSource>)actionsDataSource
         tabContextMenuDelegate:
             (id<TabContextMenuDelegate>)tabContextMenuDelegate {
  self = [super init];
  if (self) {
    _browser = browser;
    _contextMenuDelegate = tabContextMenuDelegate;
    _actionsDataSource = actionsDataSource;
  }
  return self;
}

- (UIContextMenuConfiguration*)contextMenuConfigurationForGridCell:
    (GridCell*)gridCell API_AVAILABLE(ios(13.0)) {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        if (!weakSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        GridContextMenuHelper* strongSelf = weakSelf;

        // Record that this context menu was shown to the user.
        RecordMenuShown(MenuScenario::kTabGridEntry);

        ActionFactory* actionFactory =
            [[ActionFactory alloc] initWithBrowser:strongSelf.browser
                                          scenario:MenuScenario::kTabGridEntry];

        GridItem* item = [weakSelf.actionsDataSource
            gridItemForCellIdentifier:gridCell.itemIdentifier];

        NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] init];

        [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                        [weakSelf.contextMenuDelegate shareURL:item.URL
                                                         title:item.title
                                                      fromView:gridCell];
                      }]];
        if ([weakSelf.contextMenuDelegate
                respondsToSelector:@selector(addToReadingListURL:title:)]) {
          [menuElements
              addObject:[actionFactory actionToAddToReadingListWithBlock:^{
                [weakSelf.contextMenuDelegate addToReadingListURL:item.URL
                                                            title:item.title];
              }]];
        }
        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

@end
