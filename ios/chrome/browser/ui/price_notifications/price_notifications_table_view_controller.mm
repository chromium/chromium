// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_constants.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_consumer.h"
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

using PriceNotificationItems = NSArray<PriceNotificationsTableViewItem*>*;

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
// The array of trackable items offered on the current site.
@property(nonatomic, strong) PriceNotificationItems trackableItems;
// The array of items that the user is tracking.
@property(nonatomic, strong) PriceNotificationItems trackedItems;

@end

@implementation PriceNotificationsTableViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // TODO(crbug.com/1362349) This array will be dynamically populated in a
  // future CL.
  _trackedItems = [NSArray array];

  self.title =
      l10n_util::GetNSString(IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TITLE);
  [self createTableViewHeader];

  self.tableView.accessibilityIdentifier =
      kPriceNotificationsTableViewIdentifier;
  self.tableView.estimatedSectionHeaderHeight = kSectionHeaderHeight;
  self.tableView.estimatedRowHeight = 100;
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  [self loadItemsFromArray:self.trackableItems
                 toSection:SectionIdentifierTrackableItemsOnCurrentSite];
  [self loadItemsFromArray:self.trackedItems
                 toSection:SectionIdentifierTrackedItems];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return kVerticalTableViewSectionSpacing;
}

#pragma mark - PriceNotificationsConsumer

- (void)setTrackableItem:(PriceNotificationsTableViewItem*)trackableItem
       currentlyTracking:(BOOL)currentlyTracking {
  self.trackableItems = trackableItem ? @[ trackableItem ] : @[];
  self.itemOnCurrentSiteIsTracked = currentlyTracking;

  [self loadModel];
  [self.tableView reloadData];
}

#pragma mark - Item Loading Helpers

// Adds `items` to self.tableViewModel for the section designated by
// `sectionID`.
- (void)loadItemsFromArray:(PriceNotificationItems)items
                 toSection:(SectionIdentifier)sectionID {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:sectionID];

  BOOL isItemsEmpty = !items.count;
  [model setHeader:[self createHeaderForSectionIndex:sectionID
                                             isEmpty:isItemsEmpty]
      forSectionWithIdentifier:sectionID];
  if (isItemsEmpty) {
    return;
  }

  for (PriceNotificationsTableViewItem* item in items) {
    item.type = ItemTypeListItem;
    [model addItem:item toSectionWithIdentifier:sectionID];
  }
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

@end
