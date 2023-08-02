// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_table_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_layout_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Margins used for bottom margin in "Add account" button.
// This takes into consideration the existing footer and header
// margins in ConsistencyAccountChooserTableViewController.
constexpr CGFloat kContentMargin = 16.;

}  // namespace

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

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  CGFloat width = self.tableViewController.tableView.contentSize.width;
  self.preferredContentSize =
      CGSizeMake(width, [self layoutFittingHeightForWidth:width]);
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
  CGFloat screenHeight =
      self.navigationController.view.window.bounds.size.height;
  CGFloat rowHeight = self.tableViewController.tableView.contentSize.height;
  // If `screenHeight` is undefined during a transition, use `rowHeight`.
  CGFloat height =
      screenHeight == 0 ? rowHeight : MIN(screenHeight / 2, rowHeight);
  CGFloat safeAreaInsetsHeight = 0;
  switch (self.layoutDelegate.displayStyle) {
    case ConsistencySheetDisplayStyleBottom:
      safeAreaInsetsHeight +=
          self.navigationController.view.window.safeAreaInsets.bottom;
      break;
    case ConsistencySheetDisplayStyleCentered:
      break;
  }

  // Note that there is an additional unaccounted margin height from the footer
  // and header margins that are not accounted for here.
  return self.navigationController.navigationBar.frame.size.height + height +
         kContentMargin + safeAreaInsetsHeight;
}

@end
