// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_mutator.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_view_controller_presentation_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierDataTypesHeader = kSectionIdentifierEnumZero,
  SectionIdentifierDataTypes,
};

typedef NS_ENUM(NSInteger, ItemType) {
  // SectionIdentifierDataTypesHeader
  ItemTypeHeader = kItemTypeEnumZero,
  // SectionIdentifierDataTypes
  ItemTypeModel,
};

}  // namespace

@implementation BulkUploadTableViewController {
  // View items to display.
  NSArray<BulkUploadViewItem*>* _viewItems;
}

- (void)updateViewWithViewItems:(NSArray<BulkUploadViewItem*>*)viewItems {
  _viewItems = viewItems;
  [self reloadModel];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      kBulkUploadTableViewAccessibilityIdentifier;
  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierDataTypesHeader];
  TableViewTextHeaderFooterItem* headerItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  headerItem.subtitle = l10n_util::GetNSString(
      IDS_IOS_BULK_UPLOAD_ON_THIS_DEVICE_SETTINGS_HEADER);
  [self.tableViewModel setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierDataTypesHeader];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierDataTypes];
  for (BulkUploadViewItem* viewItem in _viewItems) {
    [self addSwitchItemWithBulkUploadViewItem:viewItem];
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  // Customize height of emtpy footer for the header section to achieve desired
  // vertical spacing to next item.
  if ([self.tableViewModel sectionIdentifierForSectionIndex:section] ==
      SectionIdentifierDataTypesHeader) {
    return 0.0;
  }

  return [super tableView:tableView heightForFooterInSection:section];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.separatorInset =
      UIEdgeInsetsMake(0.f, kTableViewSeparatorInset, 0.f, 0.f);
  TableViewSwitchCell* switchCell =
      base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
  [switchCell.switchView addTarget:self
                            action:@selector(entryTapped:)
                  forControlEvents:UIControlEventTouchUpInside];
  SyncSwitchItem* switchItem = base::apple::ObjCCast<SyncSwitchItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  switchCell.switchView.tag = switchItem.dataType;
  return cell;
}

#pragma mark - Private

// Called by the UISwitch.
- (void)entryTapped:(UISwitch*)switchView {
  [self.mutator
      bulkUploadViewItemWithType:static_cast<BulkUploadType>(switchView.tag)
                      isSelected:switchView.isOn];
}

// Add a switch in the model if there are elements to upload in it.
- (void)addSwitchItemWithBulkUploadViewItem:(BulkUploadViewItem*)viewItem {
  SyncSwitchItem* switchItem =
      [[SyncSwitchItem alloc] initWithType:ItemTypeModel];
  switchItem.text = viewItem.title;
  switchItem.detailText = viewItem.subtitle;
  switchItem.on = viewItem.selected;
  switchItem.dataType = static_cast<NSInteger>(viewItem.type);
  switchItem.accessibilityIdentifier = viewItem.accessibilityIdentifier;
  [self.tableViewModel addItem:switchItem
       toSectionWithIdentifier:SectionIdentifierDataTypes];
}

// Reloads the model and the table view.
- (void)reloadModel {
  [self.tableViewModel
      deleteAllItemsFromSectionWithIdentifier:SectionIdentifierDataTypesHeader];
  [self.tableViewModel
      deleteAllItemsFromSectionWithIdentifier:SectionIdentifierDataTypes];
  for (BulkUploadViewItem* viewItem in _viewItems) {
    [self addSwitchItemWithBulkUploadViewItem:viewItem];
  }
  [self.tableView reloadData];
}

@end
