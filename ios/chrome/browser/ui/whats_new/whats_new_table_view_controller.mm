// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/whats_new/cells/whats_new_table_view_fake_header_item.h"
#import "ios/chrome/browser/ui/whats_new/cells/whats_new_table_view_item.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_action_handler.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

NSString* const kWhatsNewListViewID = @"kWhatsNewListViewId";
const CGFloat kEstimatedTableViewRowHeight = 56;
const CGFloat kEstimatedSectionHeaderHeight = 56;
const CGFloat kEstimatedsectionFooterHeight = 0.0;
const CGFloat kCellIconWidth = 64;

// List of sections.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeItem,
};

// Identifiers for sections in the What's New list.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionFeaturesIdentifier = kSectionIdentifierEnumZero,
  SectionChromeTipIdenfitier,
};

}  // namespace

@interface WhatsNewTableViewController ()

@property(nonatomic, readonly) TableViewModel<TableViewItem*>* tableViewModel;

// Array of `WhatsNewItem` features.
@property(nonatomic, strong) NSArray<WhatsNewItem*>* featureItems;

// `WhatsNewItem` representing the chrome tip.
@property(nonatomic, strong) WhatsNewItem* chromeTipItem;

// Indicates whether a scroll happened.
@property(nonatomic, assign) BOOL viewDidScroll;

@end

@implementation WhatsNewTableViewController

@dynamic tableViewModel;

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  return self;
}

#pragma mark - Public

- (void)reloadData {
  [self loadModel];
  if (self.viewLoaded)
    [self.tableView reloadData];
}

+ (NSString*)accessibilityIdentifier {
  return kWhatsNewListViewID;
}

#pragma mark - UIViewController

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  base::UmaHistogramBoolean("IOS.WhatsNew.TableViewDidScroll",
                            self.viewDidScroll);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.viewDidScroll = NO;
  self.navigationItem.backButtonTitle =
      l10n_util::GetNSString(IDS_IOS_WHATS_NEW_NAVIGATION_BACK_BUTTON_TITLE);
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_WHATS_NEW_TITLE);
  self.tableView.accessibilityIdentifier =
      [[self class] accessibilityIdentifier];
  self.tableView.estimatedRowHeight = kEstimatedTableViewRowHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedSectionHeaderHeight = kEstimatedSectionHeaderHeight;
  self.tableView.allowsMultipleSelection = YES;
  self.tableView.sectionFooterHeight = kEstimatedsectionFooterHeight;
}

#pragma mark - UITableViewactionHandler

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];

  if (item.type == ItemTypeHeader) {
    cell.separatorInset = UIEdgeInsetsMake(0.0f, 0.0f, 0.0f, CGFLOAT_MAX);
  } else {
    cell.separatorInset = UIEdgeInsetsMake(
        0,
        kTableViewHorizontalSpacing + kTableViewImagePadding + kCellIconWidth,
        0, 0);
  }
  return cell;
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (![super tableView:tableView willSelectRowAtIndexPath:indexPath]) {
    return nil;
  }

  return indexPath;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionID =
      [self.tableViewModel sectionIdentifierForSectionIndex:indexPath.section];
  switch (sectionID) {
    case SectionFeaturesIdentifier: {
      NSInteger index = indexPath.row;
      [self.actionHandler recordWhatsNewInteraction:self.featureItems[index]];
      [self.delegate detailViewController:self
          openDetailViewControllerForItem:self.featureItems[index]];
      break;
    }
    case SectionChromeTipIdenfitier: {
      [self.actionHandler recordWhatsNewInteraction:self.chromeTipItem];
      [self.delegate detailViewController:self
          openDetailViewControllerForItem:self.chromeTipItem];
      break;
    }
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  self.viewDidScroll = YES;
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  [self loadItems];
  self.tableView.alwaysBounceVertical = YES;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleSingleLine;
  self.tableView.backgroundView = nil;
}

#pragma mark - WhatsNewMediatorConsumer

- (void)setWhatsNewProperties:(WhatsNewItem*)chromeTip
                 featureItems:(NSArray<WhatsNewItem*>*)featureItems {
  self.featureItems = featureItems;
  self.chromeTipItem = chromeTip;
}

#pragma mark Private

- (TableViewItem*)whatsNewCell:(WhatsNewItem*)item {
  WhatsNewTableViewItem* cell =
      [[WhatsNewTableViewItem alloc] initWithType:ItemTypeItem];
  cell.title = item.title;
  cell.detailText = item.subtitle;
  cell.iconImage = item.iconImage;
  cell.iconBackgroundColor = item.backgroundColor;
  return cell;
}

- (TableViewItem*)defaultSectionCell {
  WhatsNewTableViewFakeHeaderItem* cell =
      [[WhatsNewTableViewFakeHeaderItem alloc] initWithType:ItemTypeHeader];
  cell.text = l10n_util::GetNSString(IDS_IOS_WHATS_NEW_SECTION_NEW_TITLE);
  return cell;
}

- (void)loadItems {
  TableViewModel* model = self.tableViewModel;

  [self loadFeatures:model];
  [self loadChromeTip:model];
}

- (void)loadFeatures:(TableViewModel*)model {
  [model addSectionWithIdentifier:SectionFeaturesIdentifier];
  [model setHeader:[self headerForSectionIndex:SectionFeaturesIdentifier]
      forSectionWithIdentifier:SectionFeaturesIdentifier];

  for (WhatsNewItem* item in self.featureItems) {
    [model addItem:[self whatsNewCell:item]
        toSectionWithIdentifier:SectionFeaturesIdentifier];
  }
}

- (void)loadChromeTip:(TableViewModel*)model {
  [model addSectionWithIdentifier:SectionChromeTipIdenfitier];
  [model setHeader:[self headerForSectionIndex:SectionChromeTipIdenfitier]
      forSectionWithIdentifier:SectionChromeTipIdenfitier];
  [model addItem:[self whatsNewCell:self.chromeTipItem]
      toSectionWithIdentifier:SectionChromeTipIdenfitier];
}

- (TableViewHeaderFooterItem*)headerForSectionIndex:
    (SectionIdentifier)sectionID {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];

  switch (sectionID) {
    case SectionFeaturesIdentifier:
      header.text = l10n_util::GetNSString(IDS_IOS_WHATS_NEW_SECTION_NEW_TITLE);
      break;
    case SectionChromeTipIdenfitier:
      header.text =
          l10n_util::GetNSString(IDS_IOS_WHATS_NEW_SECTION_CHROME_TIP_TITLE);
      break;
  }
  return header;
}

@end
