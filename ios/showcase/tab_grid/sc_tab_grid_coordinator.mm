// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_tab_grid_coordinator.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/showcase/common/protocol_alerter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCTabGridCoordinator ()<UINavigationControllerDelegate>
@property(nonatomic, strong) TabGridViewController* viewController;
@property(nonatomic, strong) ProtocolAlerter* alerter;
@end

@implementation SCTabGridCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize viewController = _viewController;
@synthesize alerter = _alerter;

- (void)start {
  self.alerter =
      [[ProtocolAlerter alloc] initWithProtocols:@[ @protocol(GridCommands) ]];
  self.viewController = [[TabGridViewController alloc]
      initWithPageConfiguration:TabGridPageConfiguration::kAllPagesEnabled];
  self.alerter.baseViewController = self.viewController;
  self.viewController.incognitoTabsDelegate =
      static_cast<id<GridCommands>>(self.alerter);
  self.viewController.regularTabsDelegate =
      static_cast<id<GridCommands>>(self.alerter);
  self.viewController.incognitoTabsDragDropHandler =
      static_cast<id<TabCollectionDragDropHandler>>(self.alerter);
  self.viewController.regularTabsDragDropHandler =
      static_cast<id<TabCollectionDragDropHandler>>(self.alerter);
  self.viewController.title = @"Full TabGrid UI";
  self.baseViewController.delegate = self;
  self.baseViewController.hidesBarsOnSwipe = YES;
  [self.baseViewController pushViewController:self.viewController animated:YES];

  NSMutableArray<TabSwitcherItem*>* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < 10; i++) {
    TabSwitcherItem* item = [[TabSwitcherItem alloc]
        initWithIdentifier:[NSString stringWithFormat:@"incogitem%d", i]];
    item.title = @"YouTube - Cat Videos";
    [items addObject:item];
  }
  [self.viewController.incognitoTabsConsumer populateItems:items
                                            selectedItemID:items[0].identifier];
  items = [[NSMutableArray alloc] init];
  for (int i = 0; i < 10; i++) {
    TabSwitcherItem* item = [[TabSwitcherItem alloc]
        initWithIdentifier:[NSString stringWithFormat:@"item%d", i]];
    item.title = @"The New York Times - Breaking News";
    [items addObject:item];
  }
  [self.viewController.regularTabsConsumer populateItems:items
                                          selectedItemID:items[0].identifier];
}

#pragma mark - UINavigationControllerDelegate

// This delegate method is used as a way to have a completion handler after
// pushing onto a navigation controller.
- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
}

@end
