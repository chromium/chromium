// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_follow_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_navigation_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_follow_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_view_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channel_item.h"
#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channels_data_source.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

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

@interface FollowManagementViewController () <UIEditMenuInteractionDelegate>

// Saved placement of the item that was last attempted to unfollow.
@property(nonatomic, strong) NSIndexPath* indexPathOfLastUnfollowAttempt;

// Saved indexPath of the item that is being selected.
@property(nonatomic, strong) NSIndexPath* indexPathOfSelectedRow;

// Saved item that was attempted to unfollow.
@property(nonatomic, strong)
    FollowedWebChannelItem* lastUnfollowedWebChannelItem;

// Used to create and show the actions users can execute when they tap on a row
// in the tableView. These actions are displayed as a pop-up.
// TODO(crbug.com/1489457): Remove available guard when min deployment target is
// bumped to iOS 16.0.
@property(nonatomic, strong)
    UIEditMenuInteraction* interactionMenu API_AVAILABLE(ios(16));

@end

@implementation FollowManagementViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self configureNavigationBar];
  [self loadModel];

  if (base::FeatureList::IsEnabled(kEnableUIEditMenuInteraction)) {
    if (@available(iOS 16.0, *)) {
      _interactionMenu = [[UIEditMenuInteraction alloc] initWithDelegate:self];
      [self.tableView addInteraction:self.interactionMenu];
    }
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self.viewDelegate followManagementViewControllerWillDismiss:self];
}

#pragma mark - UITableView

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cellToReturn = [super tableView:tableView
                             cellForRowAtIndexPath:indexPath];
  TableViewItem* tableViewItem =
      [self.tableViewModel itemAtIndexPath:indexPath];

  FollowedWebChannelItem* followedWebChannelItem =
      base::apple::ObjCCastStrict<FollowedWebChannelItem>(tableViewItem);
  FollowedWebChannelCell* followedWebChannelCell =
      base::apple::ObjCCastStrict<FollowedWebChannelCell>(cellToReturn);

  [self.faviconDataSource
      faviconForPageURL:followedWebChannelItem.URL
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

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:DefaultSectionIdentifier];

  if (IsFollowManagementInstantReloadEnabled()) {
    // Show a spinner.
    [self startLoadingIndicatorWithLoadingMessage:@""];
    // Load the followed websites.
    [self.followedWebChannelsDataSource loadFollowedWebSites];
  } else {
    NSArray<FollowedWebChannel*>* followedWebChannels =
        self.followedWebChannelsDataSource.followedWebChannels;
    for (FollowedWebChannel* followedWebChannel in followedWebChannels) {
      FollowedWebChannelItem* item = [[FollowedWebChannelItem alloc]
          initWithType:FollowedWebChannelItemType];
      item.followedWebChannel = followedWebChannel;
      [model addItem:item toSectionWithIdentifier:DefaultSectionIdentifier];
    }
    [self showOrHideEmptyTableViewBackground];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  self.indexPathOfSelectedRow = indexPath;
  if (base::FeatureList::IsEnabled(kEnableUIEditMenuInteraction) &&
      base::ios::IsRunningOnIOS16OrLater()) {
    if (@available(iOS 16.0, *)) {
      CGRect row = [tableView rectForRowAtIndexPath:indexPath];
      CGPoint editMenuLocation = CGPointMake(CGRectGetMidX(row), row.origin.y);
      UIEditMenuConfiguration* configuration = [UIEditMenuConfiguration
          configurationWithIdentifier:nil
                          sourcePoint:editMenuLocation];
      [self.interactionMenu presentEditMenuWithConfiguration:configuration];
    }
  }
#if !defined(__IPHONE_16_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_16_0
  else {
    UIMenuController* menu = [UIMenuController sharedMenuController];
    UIMenuItem* visitSiteOption = [[UIMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_FOLLOW_MANAGEMENT_VISIT_SITE_ACTION)
               action:@selector(visitSiteTapped)];
    UIMenuItem* unfollowOption = [[UIMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_FOLLOW_MANAGEMENT_UNFOLLOW_ACTION)
               action:@selector(unfollowTapped)];
    menu.menuItems = @[ visitSiteOption, unfollowOption ];

    // UIMenuController requires that this view controller be the first
    // responder in order to display the menu and handle the menu options.
    [self becomeFirstResponder];

    [menu showMenuFromView:tableView
                      rect:[tableView rectForRowAtIndexPath:indexPath]];

    // When the menu is manually presented, it doesn't get focused by
    // voiceover. This notification forces voiceover to select the
    // presented menu.
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    menu);
  }
#endif
}

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

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  __weak FollowManagementViewController* weakSelf = self;

  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    if (!weakSelf) {
      // Return an empty menu.
      return [UIMenu menuWithTitle:@"" children:@[]];
    }
    FollowManagementViewController* strongSelf = weakSelf;
    NSMutableArray<UIMenuElement*>* menuElements =
        [[NSMutableArray alloc] init];
    UIAction* unfollowSwipeAction = [UIAction
        actionWithTitle:l10n_util::GetNSString(
                            IDS_IOS_FOLLOW_MANAGEMENT_UNFOLLOW_ACTION)
                  image:nil
             identifier:nil
                handler:^(UIAction* action) {
                  [strongSelf requestUnfollowWebChannelAtIndexPath:indexPath];
                }];
    unfollowSwipeAction.attributes = UIMenuElementAttributesDestructive;
    [menuElements addObject:unfollowSwipeAction];
    return [UIMenu menuWithTitle:@"" children:menuElements];
  };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (BOOL)becomeFirstResponder {
  // starts listening for UIMenuControllerDidHideMenuNotification and triggers
  // resignFirstResponder if received.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(resignFirstResponder)
             name:UIMenuControllerDidHideMenuNotification
           object:nil];
  return [super becomeFirstResponder];
}

- (BOOL)resignFirstResponder {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIMenuControllerDidHideMenuNotification
              object:nil];
  self.indexPathOfSelectedRow = nil;
  return [super resignFirstResponder];
}

#pragma mark - Edit Menu Actions

- (void)visitSiteTapped {
  FollowedWebChannelItem* followedWebChannelItem =
      base::apple::ObjCCastStrict<FollowedWebChannelItem>(
          [self.tableViewModel itemAtIndexPath:self.indexPathOfSelectedRow]);
  const GURL& webPageURL =
      followedWebChannelItem.followedWebChannel.webPageURL.gurl;
  __weak FollowManagementViewController* weakSelf = self;
  [self dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf.navigationDelegate
                                 handleNavigateToFollowedURL:webPageURL];
                           }];
}

- (void)unfollowTapped {
  [self requestUnfollowWebChannelAtIndexPath:self.indexPathOfSelectedRow];
}

#pragma mark - FollowManagementUIUpdater

- (void)removeFollowedWebChannel:(FollowedWebChannel*)channel {
  for (UITableViewCell* cell in self.tableView.visibleCells) {
    FollowedWebChannelCell* followedWebChannelCell =
        base::apple::ObjCCastStrict<FollowedWebChannelCell>(cell);

    if ([followedWebChannelCell.followedWebChannel isEqual:channel]) {
      NSIndexPath* indexPath = [self.tableView indexPathForCell:cell];
      [followedWebChannelCell stopAnimatingActivityIndicator];
      self.lastUnfollowedWebChannelItem =
          base::apple::ObjCCastStrict<FollowedWebChannelItem>(
              [self.tableViewModel itemAtIndexPath:indexPath]);
      self.indexPathOfLastUnfollowAttempt = indexPath;
      [self deleteItemAtIndex:indexPath];
      return;
    }
  }
}

- (void)addFollowedWebChannel:(FollowedWebChannel*)channel {
  if ([self.lastUnfollowedWebChannelItem.followedWebChannel isEqual:channel]) {
    [self addItem:self.lastUnfollowedWebChannelItem
          atIndex:self.indexPathOfLastUnfollowAttempt];
  } else {
    FollowedWebChannelItem* item = [[FollowedWebChannelItem alloc]
        initWithType:FollowedWebChannelItemType];
    item.followedWebChannel = channel;

    const NSUInteger sectionIndex = [self.tableViewModel
        sectionForSectionIdentifier:DefaultSectionIdentifier];

    const NSUInteger countOfItemsInSection =
        [self.tableViewModel numberOfItemsInSection:sectionIndex];

    NSIndexPath* index = [NSIndexPath indexPathForRow:countOfItemsInSection
                                            inSection:sectionIndex];

    [self addItem:item atIndex:index];
  }
}

- (void)updateFollowedWebSites {
  CHECK(IsFollowManagementInstantReloadEnabled());

  // TODO(crbug.com/1430863): implement a timeout feature.

  // Remove the spinner.
  [self stopLoadingIndicatorWithCompletion:nil];

  // Add followed website items.
  NSArray<FollowedWebChannel*>* followedWebChannels =
      self.followedWebChannelsDataSource.followedWebChannels;
  for (FollowedWebChannel* channel in followedWebChannels) {
    FollowedWebChannelItem* item = [[FollowedWebChannelItem alloc]
        initWithType:FollowedWebChannelItemType];
    item.followedWebChannel = channel;

    const NSUInteger sectionIndex = [self.tableViewModel
        sectionForSectionIdentifier:DefaultSectionIdentifier];

    const NSUInteger countOfItemsInSection =
        [self.tableViewModel numberOfItemsInSection:sectionIndex];

    NSIndexPath* index = [NSIndexPath indexPathForRow:countOfItemsInSection
                                            inSection:sectionIndex];

    [self addItem:item atIndex:index];
  }
  [self showOrHideEmptyTableViewBackground];
}

#pragma mark - UIEditMenuInteractionDelegate

// TODO(crbug.com/1489457): Remove available guard when min deployment target is
// bumped to iOS 16.0.
- (UIMenu*)editMenuInteraction:(UIEditMenuInteraction*)interaction
          menuForConfiguration:(UIEditMenuConfiguration*)configuration
              suggestedActions:(NSArray<UIMenuElement*>*)suggestedActions
    API_AVAILABLE(ios(16)) {
  __weak FollowManagementViewController* weakSelf = self;

  UIAction* visitSite =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_FOLLOW_MANAGEMENT_VISIT_SITE_ACTION)
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction* _Nonnull action) {
                          [weakSelf visitSiteTapped];
                        }];

  UIAction* unfollow =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_FOLLOW_MANAGEMENT_UNFOLLOW_ACTION)
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction* _Nonnull action) {
                          [weakSelf unfollowTapped];
                        }];
  return [UIMenu menuWithChildren:@[ visitSite, unfollow ]];
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
  [self.feedMetricsRecorder recordManagementTappedUnfollow];

  self.indexPathOfLastUnfollowAttempt = indexPath;

  FollowedWebChannelCell* followedWebChannelCell =
      base::apple::ObjCCastStrict<FollowedWebChannelCell>(
          [self.tableView cellForRowAtIndexPath:indexPath]);
  [followedWebChannelCell startAnimatingActivityIndicator];
  [self.followDelegate
      unfollowFollowedWebChannel:followedWebChannelCell.followedWebChannel];
}

- (void)showOrHideEmptyTableViewBackground {
  TableViewModel* model = self.tableViewModel;
  NSInteger section =
      [model sectionForSectionIdentifier:DefaultSectionIdentifier];
  NSInteger itemCount = [model numberOfItemsInSection:section];
  if (itemCount == 0) {
    [self addEmptyTableViewWithImage:[UIImage imageNamed:@"following_empty"]
                               title:l10n_util::GetNSString(
                                         IDS_IOS_FOLLOW_MANAGEMENT_EMPTY_TITLE)
                            subtitle:l10n_util::GetNSString(
                                         IDS_IOS_FOLLOW_MANAGEMENT_EMPTY_TEXT)];
  } else {
    [self removeEmptyTableView];
  }
}

// Deletes item at `indexPath` from both model and UI.
- (void)deleteItemAtIndex:(NSIndexPath*)indexPath {
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
  [self showOrHideEmptyTableViewBackground];
}

- (void)addItem:(FollowedWebChannelItem*)item atIndex:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger sectionID =
      [model sectionIdentifierForSectionIndex:indexPath.section];
  [model insertItem:item
      inSectionWithIdentifier:sectionID
                      atIndex:indexPath.row];
  [self.tableView insertRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationAutomatic];

  self.lastUnfollowedWebChannelItem = nil;
  self.indexPathOfLastUnfollowAttempt = nil;
  [self showOrHideEmptyTableViewBackground];
}

@end
