// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsistencyAccountChooserViewController ()

@property(nonatomic, strong)
    ConsistencyAccountChooserTableViewController* tableViewController;

@end

@implementation ConsistencyAccountChooserViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_CHOOSE_ACCOUNT);
  [self addChildViewController:self.tableViewController];
  UIView* subView = self.tableViewController.view;
  subView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:subView];
  [NSLayoutConstraint activateConstraints:@[
    [subView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [subView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [subView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [subView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
  [self didMoveToParentViewController:self.tableViewController];
}

#pragma mark - Properties

- (id<ConsistencyAccountChooserTableViewControllerActionDelegate>)
    actionDelegate {
  return self.tableViewController.actionDelegate;
}

- (void)setActionDelegate:
    (id<ConsistencyAccountChooserTableViewControllerActionDelegate>)
        actionDelegate {
  self.tableViewController.actionDelegate = actionDelegate;
}

- (id<ConsistencyAccountChooserTableViewControllerModelDelegate>)modelDelegate {
  return self.tableViewController.modelDelegate;
}

- (void)setModelDelegate:
    (id<ConsistencyAccountChooserTableViewControllerModelDelegate>)
        modelDelegate {
  self.tableViewController.modelDelegate = modelDelegate;
}

- (id<ConsistencyAccountChooserConsumer>)consumer {
  return self.tableViewController;
}

- (ConsistencyAccountChooserTableViewController*)tableViewController {
  if (!_tableViewController) {
    _tableViewController = [[ConsistencyAccountChooserTableViewController alloc]
        initWithStyle:UITableViewStyleInsetGrouped];
  }
  return _tableViewController;
}

#pragma mark - ChildConsistencySheetViewController

- (CGFloat)layoutFittingHeightForWidth:(CGFloat)width {
  NSArray* childViewControllers =
      self.navigationController.childViewControllers;
  DCHECK(childViewControllers.count > 0);
  // Get the height of the first navigation view.
  CGFloat firstViewHeight = [childViewControllers[0] view].bounds.size.height;
  // Get the screen height.
  CGFloat screenHeight =
      self.navigationController.view.window.bounds.size.height;
  return MAX(screenHeight / 2., firstViewHeight);
}

@end
