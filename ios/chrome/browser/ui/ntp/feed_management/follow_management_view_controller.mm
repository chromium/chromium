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
  FollowedWebChannelItem* item =
      base::mac::ObjCCastStrict<FollowedWebChannelItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);

  item.followedWebChannel.unfollowRequestBlock(^(BOOL success) {
    if (success) {
      // TODO(crbug.com/1296745): Show success snackbar
      // with undo button. Also remove row.
    } else {
      // TODO(crbug.com/1296745): Show failure snackbar
      // with try again button.
    }
  });
}

@end
