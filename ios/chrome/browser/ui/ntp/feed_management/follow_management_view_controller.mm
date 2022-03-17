// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ui/follow/follow_block_types.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channel_item.h"
#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channels_data_source.h"
#include "ios/chrome/browser/ui/ntp/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// This is used by TableViewModel. This VC has one default section.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  DefaultSectionIdentifier = kSectionIdentifierEnumZero,
};

// This is used by TableViewModel. All rows are the same type of item.
typedef NS_ENUM(NSInteger, ItemType) {
  FollowedWebChannelItemType = kItemTypeEnumZero,
};

}  // namespace

@interface FollowManagementViewController ()

// Saved placement of the item that was last attempted to unfollow.
@property(nonatomic, strong) NSIndexPath* indexPathOfLastUnfollowAttempt;

// Saved item that was attempted to unfollow.
@property(nonatomic, strong)
    FollowedWebChannelItem* lastUnfollowedWebChannelItem;

@end

@implementation FollowManagementViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self configureNavigationBar];
  [self loadModel];
}

#pragma mark - UITableView

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cellToReturn = [super tableView:tableView
                             cellForRowAtIndexPath:indexPath];
  TableViewItem* tableViewItem =
      [self.tableViewModel itemAtIndexPath:indexPath];

  FollowedWebChannelItem* followedWebChannelItem =
      base::mac::ObjCCastStrict<FollowedWebChannelItem>(tableViewItem);
  FollowedWebChannelCell* followedWebChannelCell =
      base::mac::ObjCCastStrict<FollowedWebChannelCell>(cellToReturn);
  CrURL* channelURL = followedWebChannelItem.followedWebChannel.channelURL;

  [self.faviconDataSource faviconForURL:channelURL
                             completion:^(FaviconAttributes* attributes) {
                               // Only set favicon if the cell hasn't been
                               // reused.
                               if (followedWebChannelCell.followedWebChannel ==
                                   followedWebChannelItem.followedWebChannel) {
                                 DCHECK(attributes);
                                 [followedWebChannelCell.faviconView
                                     configureWithAttributes:attributes];
                               }
                             }];
  return cellToReturn;
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  NSArray<FollowedWebChannel*>* followedWebChannels =
      self.followedWebChannelsDataSource.followedWebChannels;

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:DefaultSectionIdentifier];
  for (FollowedWebChannel* followedWebChannel in followedWebChannels) {
    FollowedWebChannelItem* item = [[FollowedWebChannelItem alloc]
        initWithType:FollowedWebChannelItemType];
    item.followedWebChannel = followedWebChannel;
    [model addItem:item toSectionWithIdentifier:DefaultSectionIdentifier];
  }
}

#pragma mark - UITableViewDelegate

- (UISwipeActionsConfiguration*)tableView:(UITableView*)tableView
    trailingSwipeActionsConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath {
  UIContextualAction* unfollowSwipeAction = [UIContextualAction
      contextualActionWithStyle:UIContextualActionStyleDestructive
                          title:l10n_util::GetNSString(
                                    IDS_IOS_FOLLOW_MANAGEMENT_UNFOLLOW_ACTION)
                        handler:^(UIContextualAction* action,
                                  UIView* sourceView,
                                  void (^completionHandler)(BOOL)) {
                          [self requestUnfollowWebChannelAtIndexPath:indexPath];
                          completionHandler(YES);
                        }];
  return [UISwipeActionsConfiguration
      configurationWithActions:@[ unfollowSwipeAction ]];
}

#pragma mark - Helpers

- (void)configureNavigationBar {
  self.title = l10n_util::GetNSString(IDS_IOS_FOLLOW_MANAGEMENT_TITLE);
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                    primaryAction:[UIAction
                                      actionWithHandler:^(UIAction* action) {
                                        [self.parentViewController
                                            dismissViewControllerAnimated:YES
                                                               completion:nil];
                                      }]];
  self.navigationItem.rightBarButtonItem = doneButton;
}

- (void)requestUnfollowWebChannelAtIndexPath:(NSIndexPath*)indexPath {
  // TODO(crbug.com/1296745): Start favicon spinner.

  [self.feedMetricsRecorder recordManagementTappedUnfollow];
  self.indexPathOfLastUnfollowAttempt = indexPath;

  FollowedWebChannelItem* followedWebChannelItem =
      base::mac::ObjCCastStrict<FollowedWebChannelItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);

  __weak FollowManagementViewController* weakSelf = self;
  followedWebChannelItem.followedWebChannel.unfollowRequestBlock(
      ^(BOOL success) {
        // TODO(crbug.com/1296745): Stop favicon spinner.
        if (success) {
          // TODO(crbug.com/1296745): Show success snackbar
          // with undo button.
          weakSelf.lastUnfollowedWebChannelItem = followedWebChannelItem;
          [weakSelf deleteItemAtIndexPath:indexPath];
        } else {
          // TODO(crbug.com/1296745): Show failure snackbar
          // with try again button.
        }
      });
}

- (void)retryUnfollow {
  [self.feedMetricsRecorder recordManagementTappedUnfollowTryAgainOnSnackbar];
  [self
      requestUnfollowWebChannelAtIndexPath:self.indexPathOfLastUnfollowAttempt];
}

- (void)undoUnfollow {
  [self.feedMetricsRecorder
          recordManagementTappedRefollowAfterUnfollowOnSnackbar];

  // TODO(crbug.com/1296745): Start spinner over UNDO text in snackbar.
  FollowedWebChannelItem* unfollowedItem = self.lastUnfollowedWebChannelItem;
  unfollowedItem.followedWebChannel.refollowRequestBlock(^(BOOL success) {
    // TODO(crbug.com/1296745): Stop spinner over UNDO text in snackbar.
    if (success) {
      // TODO(crbug.com/1296745): Re-insert row.
    } else {
      // TODO(crbug.com/1296745): Show undo failure snackbar.
    }
  });
}

#pragma mark - Helpers

// Deletes item at |indexPath| from both model and UI.
- (void)deleteItemAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger sectionID =
      [model sectionIdentifierForSectionIndex:indexPath.section];
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  NSUInteger index = [model indexInItemTypeForIndexPath:indexPath];
  [model removeItemWithType:itemType
      fromSectionWithIdentifier:sectionID
                        atIndex:index];
  [self.tableView deleteRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationAutomatic];
}

@end
