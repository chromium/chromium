// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/whats_new/ui/whats_new_table_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/price_insights/model/price_insights_feature.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/whats_new/ui/cells/whats_new_table_view_item.h"
#import "ios/chrome/browser/whats_new/ui/cells/whats_new_table_view_subtitle_item.h"
#import "ios/chrome/browser/whats_new/ui/data_source/whats_new_item.h"
#import "ios/chrome/browser/whats_new/ui/whats_new_table_view_action_handler.h"
#import "ios/chrome/browser/whats_new/ui/whats_new_table_view_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_whats_new_strings.h"
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
  SectionSubtitleIdentifier,
};

}  // namespace

@interface WhatsNewTableViewController ()

@property(nonatomic, readonly) TableViewModel<TableViewItem*>* tableViewModel;

// Array of `WhatsNewItem` features.
@property(nonatomic, strong) NSArray<WhatsNewItem*>* featureItems;

// Array of `WhatsNewItem` chrome tips.
@property(nonatomic, strong) NSArray<WhatsNewItem*>* chromeTipItems;

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
  if (self.viewLoaded) {
    [self.tableView reloadData];
  }
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
  self.tableView.separatorInset = UIEdgeInsetsMake(
      0, kTableViewHorizontalSpacing + kTableViewImagePadding + kCellIconWidth,
      0, 0);
  self.tableView.estimatedRowHeight = kEstimatedTableViewRowHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedSectionHeaderHeight = kEstimatedSectionHeaderHeight;
  self.tableView.allowsMultipleSelection = YES;
  self.tableView.sectionFooterHeight = kEstimatedsectionFooterHeight;
}

#pragma mark - UITableViewDelegate

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
      NSInteger index = indexPath.row;
      [self.actionHandler recordWhatsNewInteraction:self.chromeTipItems[index]];
      [self.delegate detailViewController:self
          openDetailViewControllerForItem:self.chromeTipItems[index]];
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

- (void)setWhatsNewProperties:(NSArray<WhatsNewItem*>*)chromeTipItems
                 featureItems:(NSArray<WhatsNewItem*>*)featureItems {
  self.featureItems = featureItems;
  self.chromeTipItems = chromeTipItems;
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

  for (WhatsNewItem* item in self.chromeTipItems) {
    [model addItem:[self whatsNewCell:item]
        toSectionWithIdentifier:SectionChromeTipIdenfitier];
  }
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
    case SectionSubtitleIdentifier:
      break;
  }
  return header;
}

@end
