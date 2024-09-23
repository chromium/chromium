// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_management/feed_management_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_management/feed_management_navigation_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// These values are used in the TableViewModel to indicate sections of the
// table.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  FeedManagementSectionIdentifier = kSectionIdentifierEnumZero,
};

// These values are used in the TableViewModel to indicate specific rows.
typedef NS_ENUM(NSInteger, ItemType) {
  FollowingItemType = kItemTypeEnumZero,
  HiddenItemType,
  ActivityItemType,
};

}  // namespace

@implementation FeedManagementViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorInset, 0, 0);
  self.title = l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_TITLE);
  self.navigationController.navigationBar.prefersLargeTitles = YES;

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                    primaryAction:[UIAction
                                      actionWithHandler:^(UIAction* action) {
                                        [self.parentViewController
                                            dismissViewControllerAnimated:YES
                                                               completion:nil];
                                      }]];
  self.navigationItem.rightBarButtonItem = doneButton;

  [self loadModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.navigationController.toolbarHidden = YES;
}

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:FeedManagementSectionIdentifier];

  TableViewDetailTextItem* followingItem =
      [[TableViewDetailTextItem alloc] initWithType:FollowingItemType];
  followingItem.text =
      l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_FOLLOWING_TEXT);
  followingItem.detailText =
      l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_FOLLOWING_DETAIL);
  followingItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  followingItem.allowMultilineDetailText = YES;
  [model addItem:followingItem
      toSectionWithIdentifier:FeedManagementSectionIdentifier];

  TableViewDetailTextItem* hiddenItem =
      [[TableViewDetailTextItem alloc] initWithType:HiddenItemType];
  hiddenItem.text = l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_HIDDEN_TEXT);
  hiddenItem.detailText =
      l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_HIDDEN_DETAIL);
  hiddenItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  hiddenItem.allowMultilineDetailText = YES;
  [model addItem:hiddenItem
      toSectionWithIdentifier:FeedManagementSectionIdentifier];

  TableViewDetailTextItem* activityItem =
      [[TableViewDetailTextItem alloc] initWithType:ActivityItemType];
  activityItem.text =
      l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_ACTIVITY_TEXT);
  activityItem.detailText =
      l10n_util::GetNSString(IDS_IOS_FEED_MANAGEMENT_ACTIVITY_DETAIL);
  activityItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  activityItem.allowMultilineDetailText = YES;
  [model addItem:activityItem
      toSectionWithIdentifier:FeedManagementSectionIdentifier];
}

#pragma mark UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  __weak FeedManagementViewController* weakSelf = self;
  switch (itemType) {
    case FollowingItemType: {
      [self dismissViewControllerAnimated:YES
                               completion:^{
                                 [weakSelf.navigationDelegate
                                         handleNavigateToFollowing];
                               }];
      break;
    }
    case HiddenItemType: {
      [self dismissViewControllerAnimated:YES
                               completion:^{
                                 [weakSelf.navigationDelegate
                                         handleNavigateToHidden];
                               }];
      break;
    }
    case ActivityItemType: {
      [self dismissViewControllerAnimated:YES
                               completion:^{
                                 [weakSelf.navigationDelegate
                                         handleNavigateToActivity];
                               }];
      break;
    }
  }
}

@end
