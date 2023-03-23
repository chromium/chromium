// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/notreached.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The duration for the presentation/dismissal animation of the inactive tabs
// view.
const NSTimeInterval kDuration = 0.2;
}  // namespace

@interface InactiveTabsCoordinator () <GridViewControllerDelegate,
                                       InactiveTabsViewControllerDelegate>

// The view controller displaying the inactive tabs.
@property(nonatomic, strong) InactiveTabsViewController* viewController;

// The mediator handling the inactive tabs.
@property(nonatomic, strong) InactiveTabsMediator* mediator;

// The mutually exclusive constraints for placing `viewController`.
@property(nonatomic, strong) NSLayoutConstraint* hiddenConstraint;
@property(nonatomic, strong) NSLayoutConstraint* visibleConstraint;

@end

@implementation InactiveTabsCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  DCHECK(IsInactiveTabsEnabled());
  return [super initWithBaseViewController:viewController browser:browser];
}

- (void)start {
  [super start];

  // Create the view controller.
  self.viewController = [[InactiveTabsViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.gridViewController.delegate = self;

  // Create the mediator.
  SnapshotCache* snapshotCache =
      SnapshotBrowserAgent::FromBrowser(self.browser)->snapshot_cache();
  self.mediator = [[InactiveTabsMediator alloc]
      initWithConsumer:self.viewController.gridViewController
          webStateList:self.browser->GetWebStateList()
         snapshotCache:snapshotCache];
  self.viewController.gridViewController.imageDataSource = self.mediator;
  self.viewController.gridViewController.menuProvider = self.menuProvider;

  // Add the view controller to the hierarchy.
  UIView* baseView = self.baseViewController.view;
  UIView* view = self.viewController.view;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.baseViewController addChildViewController:self.viewController];
  [baseView addSubview:view];
  [self.viewController didMoveToParentViewController:self.baseViewController];

  self.hiddenConstraint =
      [baseView.trailingAnchor constraintEqualToAnchor:view.leadingAnchor];
  self.visibleConstraint =
      [baseView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [baseView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [baseView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [baseView.widthAnchor constraintEqualToAnchor:view.widthAnchor],
    self.hiddenConstraint,
  ]];

  [baseView layoutIfNeeded];
  [UIView animateWithDuration:kDuration
                   animations:^{
                     self.hiddenConstraint.active = NO;
                     self.visibleConstraint.active = YES;
                     [baseView layoutIfNeeded];
                   }];
}

- (void)stop {
  [super stop];

  UIView* baseView = self.baseViewController.view;

  [baseView layoutIfNeeded];
  [UIView animateWithDuration:kDuration
      animations:^{
        self.visibleConstraint.active = NO;
        self.hiddenConstraint.active = YES;
        [baseView layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        [self.viewController willMoveToParentViewController:nil];
        [self.viewController.view removeFromSuperview];
        [self.viewController removeFromParentViewController];
        self.viewController = nil;
      }];
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(GridViewController*)gridViewController
       didSelectItemWithID:(NSString*)itemID {
  [self.delegate inactiveTabsCoordinator:self didSelectItemWithID:itemID];
  [self.delegate inactiveTabsCoordinatorDidFinish:self];
}

- (void)gridViewController:(GridViewController*)gridViewController
        didCloseItemWithID:(NSString*)itemID {
  [self.mediator closeItemWithID:itemID];
}

- (void)didTapPlusSignInGridViewController:
    (GridViewController*)gridViewController {
  NOTREACHED();
}

- (void)gridViewController:(GridViewController*)gridViewController
         didMoveItemWithID:(NSString*)itemID
                   toIndex:(NSUInteger)destinationIndex {
  NOTREACHED();
}

- (void)gridViewController:(GridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count {
  // No op.
}

- (void)gridViewController:(GridViewController*)gridViewController
       didRemoveItemWIthID:(NSString*)itemID {
  // No op.
}

- (void)didChangeLastItemVisibilityInGridViewController:
    (GridViewController*)gridViewController {
  // No op.
}

- (void)gridViewController:(GridViewController*)gridViewController
    contentNeedsAuthenticationChanged:(BOOL)needsAuth {
  NOTREACHED();
}

- (void)gridViewControllerWillBeginDragging:
    (GridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerDragSessionWillBegin:
    (GridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerDragSessionDidEnd:
    (GridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerScrollViewDidScroll:
    (GridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerDropAnimationWillBegin:
    (GridViewController*)gridViewController {
  NOTREACHED();
}

- (void)gridViewControllerDropAnimationDidEnd:
    (GridViewController*)gridViewController {
  NOTREACHED();
}

- (void)didTapInactiveTabsButtonInGridViewController:
    (GridViewController*)gridViewController {
  NOTREACHED();
}

#pragma mark - InactiveTabsViewControllerDelegate

- (void)inactiveTabsViewControllerDidTapBackButton:
    (InactiveTabsViewController*)inactiveTabsViewController {
  [self.delegate inactiveTabsCoordinatorDidFinish:self];
}

@end
