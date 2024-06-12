// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_layout_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Margins used for bottom margin in "Add account" button.
// This takes into consideration the existing footer and header
// margins in AccountPickerSelectionScreenTableViewController.
constexpr CGFloat kContentMargin = 16.;

}  // namespace

@interface AccountPickerSelectionScreenViewController ()

@property(nonatomic, strong)
    AccountPickerSelectionScreenTableViewController* tableViewController;

@end

@implementation AccountPickerSelectionScreenViewController

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

- (id<AccountPickerSelectionScreenTableViewControllerActionDelegate>)
    actionDelegate {
  return self.tableViewController.actionDelegate;
}

- (void)setActionDelegate:
    (id<AccountPickerSelectionScreenTableViewControllerActionDelegate>)
        actionDelegate {
  self.tableViewController.actionDelegate = actionDelegate;
}

- (id<AccountPickerSelectionScreenTableViewControllerModelDelegate>)
    modelDelegate {
  return self.tableViewController.modelDelegate;
}

- (void)setModelDelegate:
    (id<AccountPickerSelectionScreenTableViewControllerModelDelegate>)
        modelDelegate {
  self.tableViewController.modelDelegate = modelDelegate;
}

- (id<AccountPickerSelectionScreenConsumer>)consumer {
  return self.tableViewController;
}

- (AccountPickerSelectionScreenTableViewController*)tableViewController {
  if (!_tableViewController) {
    _tableViewController =
        [[AccountPickerSelectionScreenTableViewController alloc]
            initWithStyle:UITableViewStyleInsetGrouped];
  }
  return _tableViewController;
}

#pragma mark - AccountPickerScreenViewController

- (CGFloat)layoutFittingHeightForWidth:(CGFloat)width {
  CGFloat screenHeight =
      self.navigationController.view.window.bounds.size.height;
  CGFloat rowHeight = self.tableViewController.tableView.contentSize.height;
  // If `screenHeight` is undefined during a transition, use `rowHeight`.
  CGFloat height =
      screenHeight == 0 ? rowHeight : MIN(screenHeight / 2, rowHeight);
  CGFloat safeAreaInsetsHeight = 0;
  switch (self.layoutDelegate.displayStyle) {
    case AccountPickerSheetDisplayStyle::kBottom:
      safeAreaInsetsHeight +=
          self.navigationController.view.window.safeAreaInsets.bottom;
      break;
    case AccountPickerSheetDisplayStyle::kCentered:
      break;
  }

  // Note that there is an additional unaccounted margin height from the footer
  // and header margins that are not accounted for here.
  return self.navigationController.navigationBar.frame.size.height + height +
         kContentMargin + safeAreaInsetsHeight;
}

@end
