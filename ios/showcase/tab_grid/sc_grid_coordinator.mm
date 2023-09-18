// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_grid_coordinator.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/showcase/common/protocol_alerter.h"
#import "ios/web/public/web_state_id.h"

// This Showcase-only item returns a sample image as snapshot.
@interface SCTabSwitcherItem : TabSwitcherItem
@end

@implementation SCTabSwitcherItem

- (void)fetchSnapshot:(TabSwitcherImageFetchingCompletionBlock)completion {
  completion(self, [UIImage imageNamed:@"Sample-screenshot-portrait"]);
}

@end

@interface SCGridCoordinator () <UINavigationControllerDelegate>
@property(nonatomic, strong) ProtocolAlerter* alerter;
@property(nonatomic, strong) GridViewController* gridViewController;
@end

@implementation SCGridCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize alerter = _alerter;
@synthesize gridViewController = _gridViewController;

- (void)start {
  self.baseViewController.delegate = self;
  self.alerter = [[ProtocolAlerter alloc]
      initWithProtocols:@[ @protocol(GridViewControllerDelegate) ]];
  GridViewController* gridViewController = [[GridViewController alloc] init];
  gridViewController.theme = GridThemeLight;
  gridViewController.delegate =
      static_cast<id<GridViewControllerDelegate>>(self.alerter);
  gridViewController.dragDropHandler =
      static_cast<id<TabCollectionDragDropHandler>>(self.alerter);
  self.alerter.baseViewController = gridViewController;

  NSMutableArray<TabSwitcherItem*>* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < 20; i++) {
    TabSwitcherItem* item = [[SCTabSwitcherItem alloc]
        initWithIdentifier:web::WebStateID::NewUnique()];
    item.title = @"The New York Times - Breaking News";
    [items addObject:item];
  }
  [gridViewController populateItems:items selectedItemID:items[0].identifier];
  gridViewController.title = @"Grid UI";
  [self.baseViewController pushViewController:gridViewController animated:YES];
  self.gridViewController = gridViewController;
}

#pragma mark - UINavigationControllerDelegate

// This delegate method is used as a way to have a completion handler after
// pushing onto a navigation controller.
- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
}

@end
