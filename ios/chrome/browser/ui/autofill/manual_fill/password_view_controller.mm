// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/favicon/favicon_attributes.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, ManualFallbackItemType) {
  ManualFallbackItemTypeUnkown = kItemTypeEnumZero,
  ManualFallbackItemTypeCredential,
};

namespace manual_fill {

NSString* const kPasswordDoneButtonAccessibilityIdentifier =
    @"kManualFillPasswordDoneButtonAccessibilityIdentifier";
NSString* const kPasswordSearchBarAccessibilityIdentifier =
    @"kManualFillPasswordSearchBarAccessibilityIdentifier";
NSString* const kPasswordTableViewAccessibilityIdentifier =
    @"kManualFillPasswordTableViewAccessibilityIdentifier";

}  // namespace manual_fill

@interface PasswordViewController ()

// Search controller if any.
@property(nonatomic, strong) UISearchController* searchController;

@end

@implementation PasswordViewController

- (instancetype)initWithSearchController:(UISearchController*)searchController {
  self = [super init];
  if (self) {
    _searchController = searchController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      manual_fill::kPasswordTableViewAccessibilityIdentifier;

  self.definesPresentationContext = YES;
  self.searchController.searchBar.backgroundColor = [UIColor clearColor];
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.navigationItem.searchController = self.searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.searchController.searchBar.accessibilityIdentifier =
      manual_fill::kPasswordSearchBarAccessibilityIdentifier;
  NSString* titleString =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_USE_OTHER_PASSWORD);
  self.title = titleString;

  // Center search bar vertically so it looks centered in the header when
  // searching.  The cancel button is centered / decentered on
  // viewWillAppear and viewDidDisappear.
  UIOffset offset =
      UIOffsetMake(0.0f, kTableViewNavigationVerticalOffsetForSearchHeader);
  self.searchController.searchBar.searchFieldBackgroundPositionAdjustment =
      offset;

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(handleDoneButton)];
  doneButton.accessibilityIdentifier =
      manual_fill::kPasswordDoneButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = doneButton;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Center search bar's cancel button vertically so it looks centered.
  // We change the cancel button proxy styles, so we will return it to
  // default in viewDidDisappear.
  if (self.searchController) {
    UIOffset offset =
        UIOffsetMake(0.0f, kTableViewNavigationVerticalOffsetForSearchHeader);
    UIBarButtonItem* cancelButton = [UIBarButtonItem
        appearanceWhenContainedInInstancesOfClasses:@ [[UISearchBar class]]];
    [cancelButton setTitlePositionAdjustment:offset
                               forBarMetrics:UIBarMetricsDefault];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  // Restore to default origin offset for cancel button proxy style.
  if (self.searchController) {
    UIBarButtonItem* cancelButton = [UIBarButtonItem
        appearanceWhenContainedInInstancesOfClasses:@ [[UISearchBar class]]];
    [cancelButton setTitlePositionAdjustment:UIOffsetZero
                               forBarMetrics:UIBarMetricsDefault];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  // Retrieve favicons for credential cells.
  if (itemType == ManualFallbackItemTypeCredential) {
    [self loadFaviconForCell:cell indexPath:indexPath];
  }
  return cell;
}

#pragma mark - ManualFillPasswordConsumer

- (void)presentCredentials:(NSArray<ManualFillCredentialItem*>*)credentials {
  if (self.searchController) {
    UMA_HISTOGRAM_COUNTS_1000("ManualFallback.PresentedOptions.AllPasswords",
                              credentials.count);
  } else {
    UMA_HISTOGRAM_COUNTS_100("ManualFallback.PresentedOptions.Passwords",
                             credentials.count);
  }

  for (ManualFillCredentialItem* credentialItem in credentials) {
    credentialItem.type = ManualFallbackItemTypeCredential;
  }

  // If no items were posted and there is no search bar, present the empty item
  // and return.
  if (!credentials.count && !self.searchController) {
    ManualFillActionItem* emptyCredentialItem = [[ManualFillActionItem alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_MANUAL_FALLBACK_NO_PASSWORDS_FOR_SITE)
               action:nil];
    emptyCredentialItem.enabled = NO;
    emptyCredentialItem.showSeparator = YES;
    [self presentDataItems:@[ emptyCredentialItem ]];
    return;
  }

  [self presentDataItems:credentials];
}

- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions {
  [self presentActionItems:actions];
}

#pragma mark - Private

// Retrieves favicon from FaviconLoader and sets image in |cell|.
- (void)loadFaviconForCell:(UITableViewCell*)cell
                 indexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item);
  DCHECK(cell);

  ManualFillCredentialItem* passwordItem =
      base::mac::ObjCCastStrict<ManualFillCredentialItem>(item);
  if (passwordItem.isConnectedToPreviousItem) {
    return;
  }

  ManualFillPasswordCell* passwordCell =
      base::mac::ObjCCastStrict<ManualFillPasswordCell>(cell);

  NSString* itemIdentifier = passwordItem.uniqueIdentifier;
  [self.imageDataSource
      faviconForURL:passwordItem.faviconURL
         completion:^(FaviconAttributes* attributes) {
           // Only set favicon if the cell hasn't been reused.
           if ([passwordCell.uniqueIdentifier isEqualToString:itemIdentifier]) {
             DCHECK(attributes);
             [passwordCell configureWithFaviconAttributes:attributes];
           }
         }];
}

- (void)handleDoneButton {
  [self.delegate passwordViewControllerDidTapDoneButton:self];
}

@end
