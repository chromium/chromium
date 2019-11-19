// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_popup_menu_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/badges/badge_constants.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/badges/badge_popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_consumer.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BadgePopupMenuCoordinator () <PopupMenuPresenterDelegate,
                                         PopupMenuTableViewControllerDelegate>

// The PopupMenuTableViewController managed by this coordinator.
@property(nonatomic, strong) PopupMenuTableViewController* popupViewController;

// The presenter of |popupViewController|.
@property(nonatomic, strong) PopupMenuPresenter* popupMenuPresenter;

// The consumer of the coordinator.
@property(nonatomic, weak) id<PopupMenuConsumer> consumer;

// The items to display.
@property(nonatomic, strong)
    NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>* popupMenuItems;

@end

@implementation BadgePopupMenuCoordinator

- (void)start {
  self.popupViewController = [[PopupMenuTableViewController alloc] init];
  self.popupViewController.baseViewController = self.baseViewController;
  self.popupViewController.delegate = self;
  self.popupViewController.tableView.accessibilityIdentifier =
      kBadgePopupMenuTableViewAccessibilityIdentifier;
  self.consumer = self.popupViewController;
  [self.consumer setPopupMenuItems:self.popupMenuItems];
  self.popupMenuPresenter = [[PopupMenuPresenter alloc] init];
  self.popupMenuPresenter.baseViewController = self.baseViewController;
  self.popupMenuPresenter.presentedViewController = self.popupViewController;
  self.popupMenuPresenter.guideName = kBadgeOverflowMenuGuide;
  self.popupMenuPresenter.delegate = self;
  [self.popupMenuPresenter prepareForPresentation];
  [self.popupMenuPresenter presentAnimated:YES];
}

- (void)stop {
  [self dismissPopupMenu];
  self.popupViewController = nil;
}

- (void)setBadgeItemsToShow:(NSArray<id<BadgeItem>>*)badgeItems {
  NSMutableArray<TableViewItem<PopupMenuItem>*>* items =
      [[NSMutableArray alloc] init];
  for (id<BadgeItem> item in badgeItems) {
    BadgePopupMenuItem* badgePopupMenuItem =
        [[BadgePopupMenuItem alloc] initWithBadgeType:[item badgeType]];
    [items addObject:badgePopupMenuItem];
  }
  self.popupMenuItems = @[ items ];
  [self.consumer setPopupMenuItems:self.popupMenuItems];
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  // noop.
}

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  // noop.
}

#pragma mark - PopupMenuPresenterDelegate

- (void)popupMenuPresenterWillDismiss:(PopupMenuPresenter*)presenter {
  [self dismissPopupMenu];
}

#pragma mark - PopupMenuTableViewControllerDelegate

- (void)popupMenuTableViewController:(PopupMenuTableViewController*)sender
                       didSelectItem:(TableViewItem<PopupMenuItem>*)item
                              origin:(CGPoint)origin {
  [self dismissPopupMenu];
  switch (item.actionIdentifier) {
    case PopupMenuActionShowSavePasswordOptions: {
      [self.dispatcher
          displayModalInfobar:InfobarType::kInfobarTypePasswordSave];
      break;
    }
    case PopupMenuActionShowUpdatePasswordOptions: {
      [self.dispatcher
          displayModalInfobar:InfobarType::kInfobarTypePasswordUpdate];
      break;
    }
    case PopupMenuActionShowSaveCardOptions: {
      [self.dispatcher displayModalInfobar:InfobarType::kInfobarTypeSaveCard];
      break;
    }
    default:
      NOTREACHED() << "Unexpected identifier";
      break;
  }
}

#pragma mark - Private

- (void)dismissPopupMenu {
  if (self.popupMenuPresenter) {
    [self.popupMenuPresenter dismissAnimated:YES];
    self.popupMenuPresenter = nil;
  }
}

@end
