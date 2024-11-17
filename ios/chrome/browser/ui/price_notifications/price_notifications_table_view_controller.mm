// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_table_view_controller.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_image_container_view.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_constants.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_consumer.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_mutator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

const CGFloat kVerticalTableViewSectionSpacing = 30;

// Types of ListItems used by the price notifications UI.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeSectionHeader,
  ItemTypeListItem,
  ItemTypeTableViewHeader,
};

// Identifiers for sections in the price notifications.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierTrackableItemsOnCurrentSite = kSectionIdentifierEnumZero,
  SectionIdentifierTrackedItems,
  SectionIdentifierTableViewHeader
};

const char kBookmarksSettingsURL[] = "settings://open_bookmarks";

}  // namespace

@interface PriceNotificationsTableViewController () <
    TableViewTextHeaderFooterItemDelegate>
// The boolean indicates whether there exists an item on the current site is
// already tracked or the item is already being price tracked.
@property(nonatomic, assign) BOOL itemOnCurrentSiteIsTracked;

@end

@implementation PriceNotificationsTableViewController {
  // The boolean indicates whether the user has Price tracking item
  // subscriptions displayed on the UI.
  BOOL _hasTrackedItems;
  // A boolean value that indicates that data is coming from the shoppingService
  // and that the loading state should be hidden.
  BOOL _shouldHideLoadingState;
  // A boolean value that indicates that the loading state is currently being
  // displayed.
  BOOL _displayedLoadingState;
  // Indicates whether the TableViewController's tableViewModel has been
  // initialized.
  BOOL _hasModelBeenInitialized;
  // Indicates whether the user has tracked an item over the TableView's life
  // time.
  BOOL _hasExecutedItemTracking;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self initializeTableViewModelIfNeeded];
  [self addLoadingStateItems];

  self.tableView.accessibilityIdentifier =
      kPriceNotificationsTableViewIdentifier;
  self.tableView.estimatedRowHeight = 100;
  NSUInteger imageWidthWithPadding =
      PriceNotificationsImageView::kPriceNotificationsImageLength +
      kTableViewHorizontalSpacing * 2;
  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, imageWidthWithPadding, 0, 0);

  self.title =
      l10n_util::GetNSString(IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TITLE);
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if (section ==
      [self.tableViewModel
          sectionForSectionIdentifier:SectionIdentifierTableViewHeader]) {
    return 0;
  }
  return kVerticalTableViewSectionSpacing;
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  PriceNotificationsTableViewItem* item =
      base::apple::ObjCCastStrict<PriceNotificationsTableViewItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);

  if (!item.tracking) {
    return nil;
  }

  return [super tableView:tableView willSelectRowAtIndexPath:indexPath];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
}

- (BOOL)tableView:(UITableView*)tableView
    canPerformPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  PriceNotificationsTableViewItem* item =
      base::apple::ObjCCastStrict<PriceNotificationsTableViewItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);

  return item.tracking;
}

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  PriceNotificationsTableViewItem* item =
      base::apple::ObjCCastStrict<PriceNotificationsTableViewItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);
  [self.mutator navigateToWebpageForItem:item];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* header = [super tableView:tableView viewForHeaderInSection:section];
  TableViewTextHeaderFooterView* link =
      base::apple::ObjCCast<TableViewTextHeaderFooterView>(header);
  if (link) {
    link.delegate = self;
  }

  return header;
}

#pragma mark - TableViewTextHeaderFooterItemDelegate

- (void)view:(TableViewTextHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  if (URL.gurl == GURL(kBookmarksSettingsURL)) {
    [self.mutator navigateToBookmarks];
  }
}

#pragma mark - PriceNotificationsConsumer

- (void)setTrackableItem:(PriceNotificationsTableViewItem*)trackableItem
       currentlyTracking:(BOOL)currentlyTracking {
  _shouldHideLoadingState = YES;
  [self initializeTableViewModelIfNeeded];
  [self removeLoadingState];
  self.itemOnCurrentSiteIsTracked = currentlyTracking;
  [self.tableViewModel
                     setHeader:
                         [self createHeaderForSectionIndex:
                                   SectionIdentifierTrackableItemsOnCurrentSite
                                                   isEmpty:!trackableItem]
      forSectionWithIdentifier:SectionIdentifierTrackableItemsOnCurrentSite];

  if (trackableItem && !currentlyTracking) {
    [self addItem:trackableItem
             toBeginning:YES
               ofSection:SectionIdentifierTrackableItemsOnCurrentSite
        withRowAnimation:UITableViewRowAnimationAutomatic];
  }

  if (!self.viewIfLoaded.window) {
    return;
  }

  [self.tableView
        reloadSections:[self createIndexSetForSectionIdentifiers:
                                 {SectionIdentifierTrackableItemsOnCurrentSite}]
      withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (void)addTrackedItem:(PriceNotificationsTableViewItem*)trackedItem
           toBeginning:(BOOL)beginning {
  _shouldHideLoadingState = YES;
  [self initializeTableViewModelIfNeeded];
  [self removeLoadingState];
  TableViewModel* model = self.tableViewModel;
  if (!_hasTrackedItems) {
    [model setHeader:
               [self createHeaderForSectionIndex:SectionIdentifierTrackedItems
                                         isEmpty:NO]
        forSectionWithIdentifier:SectionIdentifierTrackedItems];

    trackedItem.type = ItemTypeListItem;
    trackedItem.delegate = self;
    if (beginning) {
      [self.tableViewModel insertItem:trackedItem
              inSectionWithIdentifier:SectionIdentifierTrackedItems
                              atIndex:0];
    } else {
      [self.tableViewModel addItem:trackedItem
           toSectionWithIdentifier:SectionIdentifierTrackedItems];
    }

    if (self.viewIfLoaded.window) {
      [self.tableView reloadSections:[self createIndexSetForSectionIdentifiers:
                                               {SectionIdentifierTrackedItems}]
                    withRowAnimation:UITableViewRowAnimationFade];
    }
    _hasTrackedItems = YES;
    return;
  }

  _hasTrackedItems = YES;
  [self addItem:trackedItem
           toBeginning:beginning
             ofSection:SectionIdentifierTrackedItems
      withRowAnimation:UITableViewRowAnimationTop];
}

- (void)didStopPriceTrackingItem:(PriceNotificationsTableViewItem*)trackedItem
                   onCurrentSite:(BOOL)isViewingProductSite {
  TableViewModel* model = self.tableViewModel;
  SectionIdentifier trackedSection = SectionIdentifierTrackedItems;
  SectionIdentifier trackableSection =
      SectionIdentifierTrackableItemsOnCurrentSite;
  BOOL addItemToTrackableSection =
      isViewingProductSite && ![model hasItemForItemType:ItemTypeListItem
                                       sectionIdentifier:trackableSection];

  trackedItem.tracking = NO;
  NSIndexPath* index = [model indexPathForItem:trackedItem];
  [model removeItemWithType:ItemTypeListItem
      fromSectionWithIdentifier:trackedSection
                        atIndex:index.item];
  BOOL reloadTrackedSection = ![model hasItemForItemType:ItemTypeListItem
                                       sectionIdentifier:trackedSection];

  if (reloadTrackedSection) {
    _hasTrackedItems = NO;
    [model setHeader:
               [self createHeaderForSectionIndex:SectionIdentifierTrackedItems
                                         isEmpty:YES]
        forSectionWithIdentifier:trackedSection];
  }

  if (addItemToTrackableSection) {
    self.itemOnCurrentSiteIsTracked = NO;
    [model setHeader:[self createHeaderForSectionIndex:trackableSection
                                               isEmpty:NO]
        forSectionWithIdentifier:trackableSection];
    [model insertItem:trackedItem
        inSectionWithIdentifier:trackableSection
                        atIndex:0];
  }

  NSString* messageText = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_MENU_ITEM_STOP_TRACKING_SNACKBAR);
  MDCSnackbarMessage* message = CreateSnackbarMessage(messageText);
  [self.snackbarCommandsHandler
      showSnackbarMessage:message
           withHapticType:UINotificationFeedbackTypeSuccess];

  if (!self.viewIfLoaded.window) {
    return;
  }

  [self.tableView
      performBatchUpdates:^{
        UITableViewRowAnimation animation = UITableViewRowAnimationMiddle;
        if (addItemToTrackableSection) {
          [self.tableView
                reloadSections:[self createIndexSetForSectionIdentifiers:
                                         {trackableSection}]
              withRowAnimation:UITableViewRowAnimationFade];
          animation = UITableViewRowAnimationFade;
        }

        if (reloadTrackedSection) {
          [self.tableView
                reloadSections:
                    [self createIndexSetForSectionIdentifiers:{trackedSection}]
              withRowAnimation:UITableViewRowAnimationFade];
          return;
        }

        [self.tableView deleteRowsAtIndexPaths:@[ index ]
                              withRowAnimation:animation];
      }
               completion:^(BOOL completion){
               }];
}

- (void)didStartPriceTrackingForItem:
    (PriceNotificationsTableViewItem*)trackableItem {
  TableViewModel* model = self.tableViewModel;
  SectionIdentifier trackableSectionID =
      SectionIdentifierTrackableItemsOnCurrentSite;
  _hasExecutedItemTracking = YES;

  // This code removes the price trackable item from the trackable section and
  // adds it to the tracked section at the beginning of the list. It assumes
  // that there exists only one trackable item per page.
  [model removeItemWithType:ItemTypeListItem
      fromSectionWithIdentifier:trackableSectionID
                        atIndex:0];
  self.itemOnCurrentSiteIsTracked = YES;
  [model setHeader:[self createHeaderForSectionIndex:trackableSectionID
                                             isEmpty:YES]
      forSectionWithIdentifier:trackableSectionID];

  [self.tableView
        reloadSections:[self createIndexSetForSectionIdentifiers:
                                 {SectionIdentifierTrackableItemsOnCurrentSite}]
      withRowAnimation:UITableViewRowAnimationFade];

  [self addTrackedItem:trackableItem toBeginning:YES];
}

- (void)resetPriceTrackingItem:(PriceNotificationsTableViewItem*)item {
  PriceNotificationsTableViewCell* cell = [self.tableView
      cellForRowAtIndexPath:[self.tableViewModel indexPathForItem:item]];
  [cell.trackButton setUserInteractionEnabled:YES];
}

#pragma mark - PriceNotificationsTableViewCellDelegate

- (void)trackItemForCell:(PriceNotificationsTableViewCell*)cell {
  NSIndexPath* indexPath = [self.tableView indexPathForCell:cell];
  PriceNotificationsTableViewItem* item =
      base::apple::ObjCCastStrict<PriceNotificationsTableViewItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);
  [self.mutator trackItem:item];
}

- (void)stopTrackingItemForCell:(PriceNotificationsTableViewCell*)cell {
  NSIndexPath* indexPath = [self.tableView indexPathForCell:cell];
  PriceNotificationsTableViewItem* item =
      base::apple::ObjCCastStrict<PriceNotificationsTableViewItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);
  [self.mutator stopTrackingItem:item];
}

#pragma mark - Item Loading Helpers

// Adds an item to `sectionID` and displays it to the UI.
- (void)addItem:(PriceNotificationsTableViewItem*)item
         toBeginning:(BOOL)toBeginning
           ofSection:(SectionIdentifier)sectionID
    withRowAnimation:(UITableViewRowAnimation)animation {
  if (sectionID == SectionIdentifierTrackableItemsOnCurrentSite &&
      [self.tableViewModel
          hasItemForItemType:ItemTypeListItem
           sectionIdentifier:SectionIdentifierTrackableItemsOnCurrentSite]) {
    return;
  }

  DCHECK(item);
  item.type = ItemTypeListItem;
  item.delegate = self;
  if (toBeginning) {
    [self.tableViewModel insertItem:item
            inSectionWithIdentifier:sectionID
                            atIndex:0];
  } else {
    [self.tableViewModel addItem:item toSectionWithIdentifier:sectionID];
  }

  if (!self.viewIfLoaded.window) {
    return;
  }

  [self.tableView
      insertRowsAtIndexPaths:@[ [self.tableViewModel indexPathForItem:item] ]
            withRowAnimation:animation];
}

// Returns a TableViewHeaderFooterItem that displays the title for the section
// designated by `sectionID`.
- (TableViewHeaderFooterItem*)createHeaderForSectionIndex:
                                  (SectionIdentifier)sectionID
                                                  isEmpty:(BOOL)isEmpty {
  switch (sectionID) {
    case SectionIdentifierTrackableItemsOnCurrentSite: {
      return [self createHeaderForTrackableSection:isEmpty];
    }
    case SectionIdentifierTrackedItems: {
      return [self createHeaderForTrackedSection:isEmpty];
    }
    case SectionIdentifierTableViewHeader: {
      return [self createHeaderForTableViewHeaderSection:isEmpty];
    }
  }

  return nil;
}

// Constructs the TableViewModel's sections and section headers and initializes
// their values in accordance with the desired default empty state.
- (void)initializeTableViewModelIfNeeded {
  if (_hasModelBeenInitialized) {
    return;
  }

  _hasModelBeenInitialized = YES;
  [super loadModel];
  SectionIdentifier trackableSectionID =
      SectionIdentifierTrackableItemsOnCurrentSite;
  SectionIdentifier trackedSectionID = SectionIdentifierTrackedItems;
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierTableViewHeader];
  [model setHeader:
             [self createHeaderForSectionIndex:SectionIdentifierTableViewHeader
                                       isEmpty:YES]
      forSectionWithIdentifier:SectionIdentifierTableViewHeader];

  [model addSectionWithIdentifier:trackableSectionID];
  [model setHeader:[self createHeaderForSectionIndex:trackableSectionID
                                             isEmpty:NO]
      forSectionWithIdentifier:trackableSectionID];

  [model addSectionWithIdentifier:trackedSectionID];
  [model setHeader:[self createHeaderForSectionIndex:trackedSectionID
                                             isEmpty:YES]
      forSectionWithIdentifier:trackedSectionID];
}

// Adds placeholder items into each section.
- (void)addLoadingStateItems {
  if (_shouldHideLoadingState) {
    return;
  }

  [self.tableViewModel setHeader:[self createHeaderForSectionIndex:
                                           SectionIdentifierTrackedItems
                                                           isEmpty:NO]
        forSectionWithIdentifier:SectionIdentifierTrackedItems];

  PriceNotificationsTableViewItem* emptyTrackableItem =
      [[PriceNotificationsTableViewItem alloc] init];
  PriceNotificationsTableViewItem* emptyTrackedItem =
      [[PriceNotificationsTableViewItem alloc] init];
  emptyTrackableItem.loading = YES;
  emptyTrackedItem.loading = YES;
  emptyTrackedItem.tracking = YES;
  [self addItem:emptyTrackableItem
           toBeginning:YES
             ofSection:SectionIdentifierTrackableItemsOnCurrentSite
      withRowAnimation:UITableViewRowAnimationAutomatic];
  [self addItem:emptyTrackedItem
           toBeginning:YES
             ofSection:SectionIdentifierTrackedItems
      withRowAnimation:UITableViewRowAnimationAutomatic];
  [self addItem:emptyTrackedItem
           toBeginning:YES
             ofSection:SectionIdentifierTrackedItems
      withRowAnimation:UITableViewRowAnimationAutomatic];
  _displayedLoadingState = YES;
}

// Removes the placeholder items from each section.
- (void)removeLoadingState {
  if (!_displayedLoadingState) {
    return;
  }

  _displayedLoadingState = NO;
  TableViewModel* model = self.tableViewModel;

  NSMutableArray<NSIndexPath*>* itemIndexPaths =
      [[model indexPathsForItemType:ItemTypeListItem
                  sectionIdentifier:SectionIdentifierTrackedItems] mutableCopy];
  [itemIndexPaths
      addObject:[model indexPathForItemType:ItemTypeListItem
                          sectionIdentifier:
                              SectionIdentifierTrackableItemsOnCurrentSite]];

  [model removeItemWithType:ItemTypeListItem
      fromSectionWithIdentifier:SectionIdentifierTrackableItemsOnCurrentSite
                        atIndex:0];
  [model setHeader:[self
                       createHeaderForSectionIndex:SectionIdentifierTrackedItems
                                           isEmpty:YES]
      forSectionWithIdentifier:SectionIdentifierTrackedItems];
  [model deleteAllItemsFromSectionWithIdentifier:SectionIdentifierTrackedItems];

  if (!self.viewIfLoaded.window) {
    return;
  }

  [self.tableView deleteRowsAtIndexPaths:itemIndexPaths
                        withRowAnimation:UITableViewRowAnimationAutomatic];
  [self.tableView reloadSections:[self createIndexSetForSectionIdentifiers:
                                           {SectionIdentifierTrackedItems}]
                withRowAnimation:UITableViewRowAnimationAutomatic];
}

// Creates the IndexSet that encapsulate the various sections provided in
// `sectionIDs`
- (NSIndexSet*)createIndexSetForSectionIdentifiers:
    (std::vector<SectionIdentifier>)sectionIDs {
  NSMutableIndexSet* indexSet = [[NSMutableIndexSet alloc] init];
  for (auto& sectionID : sectionIDs) {
    NSUInteger sectionIndex =
        [self.tableViewModel sectionForSectionIdentifier:sectionID];
    [indexSet addIndex:sectionIndex];
  }
  return indexSet;
}

// Creates the TableViewHeaderFooterItem for the section
// `SectionIdentifierTrackableItemsOnCurrentSite`
- (TableViewHeaderFooterItem*)createHeaderForTrackableSection:(BOOL)isEmpty {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKABLE_SECTION_HEADER);
  if (!self.itemOnCurrentSiteIsTracked && !isEmpty) {
    return header;
  }

  if (self.itemOnCurrentSiteIsTracked && _hasExecutedItemTracking) {
    header.subtitle = l10n_util::GetNSString(
        IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_DESCRIPTION_FOR_TRACKED_ITEM);
    header.URLs = @[ [[CrURL alloc] initWithGURL:GURL(kBookmarksSettingsURL)] ];
    return header;
  }

  if (self.itemOnCurrentSiteIsTracked) {
    header.subtitle = l10n_util::GetNSString(
        IDS_IOS_PRICE_NOTIFICAITONS_PRICE_TRACK_TRACKABLE_ITEM_IS_TRACKED);
    return header;
  }

  header.subtitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKABLE_EMPTY_LIST);
  return header;
}

// Creates the TableViewHeaderFooterItem for the section
// `SectionIdentifierTrackedItems`
- (TableViewHeaderFooterItem*)createHeaderForTrackedSection:(BOOL)isEmpty {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKED_SECTION_HEADER);
  if (isEmpty) {
    header.subtitle = l10n_util::GetNSString(
        IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKING_EMPTY_LIST);
    return header;
  }

  return header;
}

// Creates the TableViewHeaderFooterItem for the section
// `SectionIdentifierTableViewHeader`
- (TableViewTextHeaderFooterItem*)createHeaderForTableViewHeaderSection:
    (BOOL)isEmpty {
  TableViewTextHeaderFooterItem* header = [[TableViewTextHeaderFooterItem alloc]
      initWithType:ItemTypeTableViewHeader];

  if (isEmpty && !self.hasPreviouslyViewed) {
    header.subtitle = l10n_util::GetNSString(
        IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_DESCRIPTION_EMPTY_STATE);
    return header;
  }

  header.subtitle = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_DESCRIPTION);
  return header;
}

@end
