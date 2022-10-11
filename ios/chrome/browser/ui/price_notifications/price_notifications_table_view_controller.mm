// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_constants.h"
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
}  // namespace

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

@interface PriceNotificationsTableViewController () {
  // The header for the TableView.
  TableViewLinkHeaderFooterItem* _descriptionText;
}

@end

@implementation PriceNotificationsTableViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title =
      l10n_util::GetNSString(IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TITLE);
  [self createTableViewHeader];
  [self loadModel];

  self.tableView.accessibilityIdentifier =
      kPriceNotificationsTableViewIdentifier;
  self.tableView.estimatedSectionHeaderHeight = kSectionHeaderHeight;
  self.tableView.estimatedRowHeight = 100;
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  [self loadItems];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return kVerticalTableViewSectionSpacing;
}

#pragma mark - Item Loading Helpers

// Loads the TableViewItems into self.tableViewModel.
- (void)loadItems {
  NSMutableArray<PriceNotificationsTableViewItem*>* trackableArray =
      [NSMutableArray array];
  NSMutableArray<PriceNotificationsTableViewItem*>* trackedArray =
      [NSMutableArray array];
  // TODO(crbug.com/1373071) Once the PriceNotificationsMediator has been
  // created, the mediator will be called to populate the `trackableArray` and
  // `trackedArray` objects.
  [self loadItemsFromArray:trackableArray
                 toSection:SectionIdentifierTrackableItemsOnCurrentSite];
  [self loadItemsFromArray:trackedArray
                 toSection:SectionIdentifierTrackedItems];
}

// Adds `items` to self.tableViewModel for the section designated by
// `sectionID`.
- (void)loadItemsFromArray:(NSArray<PriceNotificationsTableViewItem*>*)items
                 toSection:(SectionIdentifier)sectionID {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:sectionID];

  if (!items.count) {
    [model setHeader:[self createHeaderForSectionIndex:sectionID isEmpty:YES]
        forSectionWithIdentifier:sectionID];
    return;
  }

  [model setHeader:[self createHeaderForSectionIndex:sectionID isEmpty:NO]
      forSectionWithIdentifier:sectionID];
  for (PriceNotificationsTableViewItem* item in items) {
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
      if (isEmpty) {
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

  AddSameConstraintsWithInsets(description, view,
                               ChromeDirectionalEdgeInsetsMake(
                                   kDescriptionPadding, kDescriptionPadding,
                                   kDescriptionPadding, kDescriptionPadding));
}

@end
