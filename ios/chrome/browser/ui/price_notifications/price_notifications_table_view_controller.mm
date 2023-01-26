// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_table_view_controller.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_constants.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_consumer.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_mutator.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kVerticalTableViewSectionSpacing = 30;
const CGFloat kDescriptionPadding = 16;
const CGFloat kDescriptionLabelHeight = 18;
const CGFloat kDescriptionTopSpacing = 10;
const CGFloat kDescriptionBottomSpacing = 26;
const CGFloat kSectionHeaderHeight = 22;

// Types of ListItems used by the price notifications UI.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeSectionHeader,
  ItemTypeListItem,
};

// Identifiers for sections in the price notifications.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierTrackableItemsOnCurrentSite = kSectionIdentifierEnumZero,
  SectionIdentifierTrackedItems,
};

}  // namespace

@interface PriceNotificationsTableViewController ()
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
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self initializeTableViewModelIfNeeded];
  [self addLoadingStateItems];

  self.tableView.accessibilityIdentifier =
      kPriceNotificationsTableViewIdentifier;
  self.tableView.estimatedSectionHeaderHeight = kSectionHeaderHeight;
  self.tableView.estimatedRowHeight = 100;

  self.title =
      l10n_util::GetNSString(IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TITLE);
  [self createTableViewHeader];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return kVerticalTableViewSectionSpacing;
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
        toSection:SectionIdentifierTrackableItemsOnCurrentSite];
  }

  if (!self.viewIfLoaded.window) {
    return;
  }

  [self.tableView
        reloadSections:
            [NSIndexSet
                indexSetWithIndexesInRange:
                    [self createRangeForSectionIdentifier:
                              SectionIdentifierTrackableItemsOnCurrentSite]]
      withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (void)addTrackedItem:(PriceNotificationsTableViewItem*)trackedItem {
  _shouldHideLoadingState = YES;
  [self initializeTableViewModelIfNeeded];
  [self removeLoadingState];

  TableViewModel* model = self.tableViewModel;
  BOOL shouldReloadSection = NO;

  if (!_hasTrackedItems) {
    [model setHeader:
               [self createHeaderForSectionIndex:SectionIdentifierTrackedItems
                                         isEmpty:NO]
        forSectionWithIdentifier:SectionIdentifierTrackedItems];
    shouldReloadSection = YES;
  }

  _hasTrackedItems = YES;
  [self addItem:trackedItem toSection:SectionIdentifierTrackedItems];

  if (!self.viewIfLoaded.window || !shouldReloadSection) {
    return;
  }

  [self.tableView
        reloadSections:[NSIndexSet indexSetWithIndexesInRange:
                                       [self createRangeForSectionIdentifier:
                                                 SectionIdentifierTrackedItems]]
      withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (void)didStopPriceTrackingItem:(PriceNotificationsTableViewItem*)trackedItem
                   onCurrentSite:(BOOL)isViewingProductSite {
  [self.tableView
      performBatchUpdates:^{
        TableViewModel* model = self.tableViewModel;
        SectionIdentifier trackedSection = SectionIdentifierTrackedItems;

        NSIndexPath* index = [model indexPathForItem:trackedItem];
        [model removeItemWithType:ItemTypeListItem
            fromSectionWithIdentifier:trackedSection
                              atIndex:index.item];
        [self.tableView
            deleteRowsAtIndexPaths:@[ index ]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
        trackedItem.tracking = NO;

        if (![model hasItemForItemType:ItemTypeListItem
                     sectionIdentifier:trackedSection]) {
          _hasTrackedItems = NO;
          [model setHeader:[self createHeaderForSectionIndex:
                                     SectionIdentifierTrackedItems
                                                     isEmpty:YES]
              forSectionWithIdentifier:trackedSection];
          [self.tableView
                reloadSections:[NSIndexSet
                                   indexSetWithIndexesInRange:
                                       [self createRangeForSectionIdentifier:
                                                 SectionIdentifierTrackedItems]]
              withRowAnimation:UITableViewRowAnimationAutomatic];
        }

        if (isViewingProductSite) {
          [self setTrackableItem:trackedItem currentlyTracking:NO];
        }
      }
               completion:nil];
  NSString* messageText = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_MENU_ITEM_STOP_TRACKING);
  MDCSnackbarMessage* message =
      [MDCSnackbarMessage messageWithText:messageText];
  [self.snackbarCommandsHandler
      showSnackbarMessage:message
           withHapticType:UINotificationFeedbackTypeSuccess];
}

- (void)didStartPriceTrackingForItem:
    (PriceNotificationsTableViewItem*)trackableItem {
  TableViewModel* model = self.tableViewModel;
  SectionIdentifier trackableSectionID =
      SectionIdentifierTrackableItemsOnCurrentSite;

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
        reloadSections:
            [NSIndexSet
                indexSetWithIndexesInRange:
                    [self createRangeForSectionIdentifier:
                              SectionIdentifierTrackableItemsOnCurrentSite]]
      withRowAnimation:UITableViewRowAnimationAutomatic];

  [self addTrackedItem:trackableItem];
}

#pragma mark - PriceNotificationsTableViewCellDelegate

- (void)trackItemForCell:(PriceNotificationsTableViewCell*)cell {
  NSIndexPath* indexPath = [self.tableView indexPathForCell:cell];
  PriceNotificationsTableViewItem* item =
      base::mac::ObjCCastStrict<PriceNotificationsTableViewItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);
  [self.mutator trackItem:item];
}

- (void)stopTrackingItemForCell:(PriceNotificationsTableViewCell*)cell {
  NSIndexPath* indexPath = [self.tableView indexPathForCell:cell];
  PriceNotificationsTableViewItem* item =
      base::mac::ObjCCastStrict<PriceNotificationsTableViewItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);
  [self.mutator stopTrackingItem:item];
}

#pragma mark - Item Loading Helpers

// Adds an item to `sectionID` and displays it to the UI.
- (void)addItem:(PriceNotificationsTableViewItem*)item
      toSection:(SectionIdentifier)sectionID {
  DCHECK(item);
  item.type = ItemTypeListItem;
  item.delegate = self;
  [self.tableViewModel insertItem:item
          inSectionWithIdentifier:sectionID
                          atIndex:0];

  if (!self.viewIfLoaded.window) {
    return;
  }

  [self.tableView
      insertRowsAtIndexPaths:@[ [self.tableViewModel indexPathForItem:item] ]
            withRowAnimation:UITableViewRowAnimationAutomatic];
}

// Returns a TableViewHeaderFooterItem that displays the title for the section
// designated by `sectionID`.
- (TableViewHeaderFooterItem*)createHeaderForSectionIndex:
                                  (SectionIdentifier)sectionID
                                                  isEmpty:(BOOL)isEmpty {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];

  switch (sectionID) {
    case SectionIdentifierTrackableItemsOnCurrentSite:
      header.text = l10n_util::GetNSString(
          IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKABLE_SECTION_HEADER);
      if (self.itemOnCurrentSiteIsTracked) {
        header.subtitleText = l10n_util::GetNSString(
            IDS_IOS_PRICE_NOTIFICAITONS_PRICE_TRACK_TRACKABLE_ITEM_IS_TRACKED);
      } else if (isEmpty) {
        header.subtitleText = l10n_util::GetNSString(
            IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKABLE_EMPTY_LIST);
      }
      break;
    case SectionIdentifierTrackedItems:
      header.text = l10n_util::GetNSString(
          IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKED_SECTION_HEADER);
      if (isEmpty) {
        header.subtitleText = l10n_util::GetNSString(
            IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKING_EMPTY_LIST);
      }
      break;
  }

  return header;
}

// Constructs the TableViewModel's sections and section headers and initializes
// their values in accordance with the desired default empty state.
- (void)initializeTableViewModelIfNeeded {
  if (self.tableViewModel) {
    return;
  }

  [super loadModel];
  SectionIdentifier trackableSectionID =
      SectionIdentifierTrackableItemsOnCurrentSite;
  SectionIdentifier trackedSectionID = SectionIdentifierTrackedItems;
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:trackableSectionID];
  [model setHeader:[self createHeaderForSectionIndex:trackableSectionID
                                             isEmpty:NO]
      forSectionWithIdentifier:trackableSectionID];

  [model addSectionWithIdentifier:trackedSectionID];
  [model setHeader:[self createHeaderForSectionIndex:trackedSectionID
                                             isEmpty:YES]
      forSectionWithIdentifier:trackedSectionID];
}

// Creates, configures, and sets a UIView to the TableViewControllers's
// TableHeaderView property.
- (void)createTableViewHeader {
  UIView* view =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0,
                                               kDescriptionLabelHeight +
                                                   kDescriptionTopSpacing +
                                                   kDescriptionBottomSpacing)];
  UILabel* description = [[UILabel alloc] init];
  description.translatesAutoresizingMaskIntoConstraints = NO;
  description.font =
      [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
  description.textColor = [UIColor colorNamed:kTextSecondaryColor];
  description.text = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_DESCRIPTION);

  [view addSubview:description];
  self.tableView.tableHeaderView = view;

  AddSameConstraintsWithInsets(
      description, view,
      NSDirectionalEdgeInsetsMake(kDescriptionPadding, kDescriptionPadding,
                                  kDescriptionPadding, kDescriptionPadding));
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
      toSection:SectionIdentifierTrackableItemsOnCurrentSite];
  [self addItem:emptyTrackedItem toSection:SectionIdentifierTrackedItems];
  [self addItem:emptyTrackedItem toSection:SectionIdentifierTrackedItems];
  _displayedLoadingState = YES;
}

// Removes the placeholder items from each section.
- (void)removeLoadingState {
  if (!_displayedLoadingState) {
    return;
  }

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
  [self.tableView
        reloadSections:[NSIndexSet indexSetWithIndexesInRange:
                                       [self createRangeForSectionIdentifier:
                                                 SectionIdentifierTrackedItems]]
      withRowAnimation:UITableViewRowAnimationAutomatic];
  _displayedLoadingState = NO;
}

// Creates the NSRanges that encapsulate the various sections within the table
// view.
- (NSRange)createRangeForSectionIdentifier:(SectionIdentifier)sectionID {
  switch (sectionID) {
    case SectionIdentifierTrackableItemsOnCurrentSite:
      return NSMakeRange(0, 1);
    case SectionIdentifierTrackedItems:
      return NSMakeRange(1, 1);
  }
}

@end
