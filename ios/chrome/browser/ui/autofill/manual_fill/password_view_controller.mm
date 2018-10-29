// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"

#include "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manual_fill {

NSString* const PasswordSearchBarAccessibilityIdentifier =
    @"kManualFillPasswordSearchBarAccessibilityIdentifier";
NSString* const PasswordTableViewAccessibilityIdentifier =
    @"kManualFillPasswordTableViewAccessibilityIdentifier";

}  // namespace manual_fill

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  CredentialsSectionIdentifier = kSectionIdentifierEnumZero,
  ActionsSectionIdentifier,
};

// This is the width used for |self.preferredContentSize|.
constexpr float PopoverPreferredWidth = 320;

// This is the maximum height used for |self.preferredContentSize|.
constexpr float PopoverMaxHeight = 250;

}  // namespace

@interface PasswordViewController ()
// Search controller if any.
@property(nonatomic, strong) UISearchController* searchController;
@end

@implementation PasswordViewController

@synthesize searchController = _searchController;

- (instancetype)initWithSearchController:(UISearchController*)searchController {
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _searchController = searchController;

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleKeyboardWillShow:)
               name:UIKeyboardWillShowNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleKeyboardDidHide:)
               name:UIKeyboardDidHideNotification
             object:nil];
  }
  return self;
}

- (void)viewDidLoad {
  // Super's |viewDidLoad| uses |styler.tableViewBackgroundColor| so it needs to
  // be set before.
  self.styler.tableViewBackgroundColor = [UIColor whiteColor];

  [super viewDidLoad];

  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 20.0;
  self.tableView.estimatedRowHeight = 200;
  self.tableView.separatorInset = UIEdgeInsetsMake(0, 0, 0, 0);
  self.tableView.accessibilityIdentifier =
      manual_fill::PasswordTableViewAccessibilityIdentifier;

  self.definesPresentationContext = YES;
  self.searchController.searchBar.backgroundColor = [UIColor clearColor];
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  if (@available(iOS 11, *)) {
    self.navigationItem.searchController = self.searchController;
    self.navigationItem.hidesSearchBarWhenScrolling = NO;
  } else {
    self.tableView.tableHeaderView = self.searchController.searchBar;
  }
  self.searchController.searchBar.accessibilityIdentifier =
      manual_fill::PasswordSearchBarAccessibilityIdentifier;
  NSString* titleString =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_USE_OTHER_PASSWORD);
  self.title = titleString;

  if (!base::ios::IsRunningOnIOS11OrLater()) {
    // On iOS 11 this is not needed since the cell constrains are updated by the
    // system.
    [[NSNotificationCenter defaultCenter]
        addObserver:self.tableView
           selector:@selector(reloadData)
               name:UIContentSizeCategoryDidChangeNotification
             object:nil];
  }
}

#pragma mark - ManualFillPasswordConsumer

- (void)presentCredentials:(NSArray<ManualFillCredentialItem*>*)credentials {
  [self presentItems:credentials inSection:CredentialsSectionIdentifier];
}

- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions {
  [self presentItems:actions inSection:ActionsSectionIdentifier];
}

#pragma mark - Private

- (void)handleKeyboardDidHide:(NSNotification*)notification {
  if (self.contentInsetsAlwaysEqualToSafeArea && !IsIPadIdiom()) {
    // Resets the table view content inssets to be equal to the safe area
    // insets.
    self.tableView.contentInset = SafeAreaInsetsForView(self.view);
  }
}

- (void)handleKeyboardWillShow:(NSNotification*)notification {
  if (self.contentInsetsAlwaysEqualToSafeArea && !IsIPadIdiom()) {
    // Sets the bottom inset to be equal to the height of the keyboard to
    // override the behaviour in UITableViewController. Which adjust the scroll
    // view insets to accommodate for the keyboard.
    CGRect keyboardFrame =
        [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    CGFloat keyboardHeight = keyboardFrame.size.height;
    UIEdgeInsets safeInsets = SafeAreaInsetsForView(self.view);
    self.tableView.contentInset =
        UIEdgeInsetsMake(safeInsets.top, safeInsets.left,
                         safeInsets.bottom - keyboardHeight, safeInsets.right);
  }
}

// Presents |items| in the respective section. Handles creating or deleting the
// section accordingly.
- (void)presentItems:(NSArray<TableViewItem*>*)items
           inSection:(SectionIdentifier)sectionIdentifier {
  if (!self.tableViewModel) {
    [self loadModel];
  }
  BOOL doesSectionExist =
      [self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier];
  // If there are no passed credentials, remove section if exist.
  if (!items.count) {
    if (doesSectionExist) {
      [self.tableViewModel removeSectionWithIdentifier:sectionIdentifier];
    }
  } else {
    if (!doesSectionExist) {
      if (sectionIdentifier == CredentialsSectionIdentifier) {
        [self.tableViewModel
            insertSectionWithIdentifier:CredentialsSectionIdentifier
                                atIndex:0];
      } else {
        [self.tableViewModel addSectionWithIdentifier:sectionIdentifier];
      }
    }
    [self.tableViewModel
        deleteAllItemsFromSectionWithIdentifier:sectionIdentifier];
    for (TableViewItem* item in items) {
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:sectionIdentifier];
    }
  }
  [self.tableView reloadData];
  if (IsIPadIdiom()) {
    // Update the preffered content size on iPad so the popover shows the right
    // size.
    [self.tableView layoutIfNeeded];
    CGSize systemLayoutSize = self.tableView.contentSize;
    CGFloat preferredHeight = MIN(systemLayoutSize.height, PopoverMaxHeight);
    self.preferredContentSize =
        CGSizeMake(PopoverPreferredWidth, preferredHeight);
  }
}

@end
