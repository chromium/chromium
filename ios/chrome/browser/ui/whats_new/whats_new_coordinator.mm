// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_coordinator.h"

#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_mediator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kTableViewNavigationDismissButtonId =
    @"kWhatsNewTableViewNavigationDismissButtonId";

}  // namespace

@interface WhatsNewCoordinator ()

// The mediator to display What's New data.
@property(nonatomic, strong) WhatsNewMediator* mediator;
// The navigation controller displaying WhatsNewTableViewController.
@property(nonatomic, strong)
    TableViewNavigationController* navigationController;
// The view controller used to display the What's New features and chrome tips.
@property(nonatomic, strong) WhatsNewTableViewController* tableViewController;

@end

@implementation WhatsNewCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  self.mediator = [[WhatsNewMediator alloc] init];
  self.tableViewController = [[WhatsNewTableViewController alloc] init];
  self.tableViewController.navigationItem.rightBarButtonItem =
      [self dismissButton];

  self.tableViewController.delegate = self.mediator;
  self.mediator.consumer = self.tableViewController;

  [self.tableViewController reloadData];

  self.navigationController = [[TableViewNavigationController alloc]
      initWithTable:self.tableViewController];
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];

  [super start];
}

- (void)stop {
  self.mediator = nil;
  [super stop];
}

#pragma mark Private

- (UIBarButtonItem*)dismissButton {
  UIBarButtonItem* button = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissButtonTapped)];
  [button setAccessibilityIdentifier:kTableViewNavigationDismissButtonId];
  return button;
}

- (void)dismissButtonTapped {
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.tableViewController = nil;
  self.navigationController = nil;

  [self stop];
}

@end
