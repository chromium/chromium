// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/settings/cells/settings_multiline_detail_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The accessibility identifier of the Import Data cell.
NSString* const kImportDataImportCellId = @"kImportDataImportCellId";
// The accessibility identifier of the Keep Data Separate cell.
NSString* const kImportDataKeepSeparateCellId =
    @"kImportDataKeepSeparateCellId";
NSString* const kImportDataContinueButtonId = @"kImportDataContinueButtonId";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierDisclaimer = kSectionIdentifierEnumZero,
  SectionIdentifierOptions,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFooter = kItemTypeEnumZero,
  ItemTypeOptionImportData,
  ItemTypeOptionKeepDataSeparate,
};

}  // namespace

@implementation ImportDataTableViewController {
  __weak id<ImportDataControllerDelegate> _delegate;
  NSString* _fromEmail;
  NSString* _toEmail;
  BOOL _isSignedIn;
  ShouldClearData _shouldClearData;
  SettingsMultilineDetailItem* _importDataItem;
  SettingsMultilineDetailItem* _keepDataSeparateItem;
}

#pragma mark - Initialization

- (instancetype)initWithDelegate:(id<ImportDataControllerDelegate>)delegate
                       fromEmail:(NSString*)fromEmail
                         toEmail:(NSString*)toEmail
                      isSignedIn:(BOOL)isSignedIn {
  DCHECK(fromEmail);
  DCHECK(toEmail);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _delegate = delegate;
    _fromEmail = [fromEmail copy];
    _toEmail = [toEmail copy];
    _isSignedIn = isSignedIn;
    _shouldClearData = isSignedIn ? SHOULD_CLEAR_DATA_CLEAR_DATA
                                  : SHOULD_CLEAR_DATA_MERGE_DATA;
    self.title =
        isSignedIn
            ? l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_TITLE_SWITCH)
            : l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_TITLE_SIGNIN);
    [self setShouldHideDoneButton:YES];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setShouldHideDoneButton:YES];
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_OPTIONS_IMPORT_DATA_CONTINUE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(didTapContinue)];
  self.navigationItem.rightBarButtonItem.accessibilityIdentifier =
      kImportDataContinueButtonId;
  [self loadModel];
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierDisclaimer];
  [model addItem:[self descriptionItem]
      toSectionWithIdentifier:SectionIdentifierDisclaimer];

  [model addSectionWithIdentifier:SectionIdentifierOptions];
  _importDataItem = [self importDataItem];
  _keepDataSeparateItem = [self keepDataSeparateItem];
  if (_isSignedIn) {
    [model addItem:_keepDataSeparateItem
        toSectionWithIdentifier:SectionIdentifierOptions];
    [model addItem:_importDataItem
        toSectionWithIdentifier:SectionIdentifierOptions];
  } else {
    [model addItem:_importDataItem
        toSectionWithIdentifier:SectionIdentifierOptions];
    [model addItem:_keepDataSeparateItem
        toSectionWithIdentifier:SectionIdentifierOptions];
  }
}

#pragma mark - Items

- (TableViewItem*)descriptionItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeFooter];
  item.text = l10n_util::GetNSStringF(IDS_IOS_OPTIONS_IMPORT_DATA_HEADER,
                                      base::SysNSStringToUTF16(_fromEmail));
  return item;
}

- (SettingsMultilineDetailItem*)importDataItem {
  SettingsMultilineDetailItem* item = [[SettingsMultilineDetailItem alloc]
      initWithType:ItemTypeOptionImportData];
  item.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_IMPORT_TITLE);
  item.detailText =
      l10n_util::GetNSStringF(IDS_IOS_OPTIONS_IMPORT_DATA_IMPORT_SUBTITLE,
                              base::SysNSStringToUTF16(_toEmail));
  item.accessoryType = _isSignedIn ? UITableViewCellAccessoryNone
                                   : UITableViewCellAccessoryCheckmark;
  item.accessibilityIdentifier = kImportDataImportCellId;
  return item;
}

- (SettingsMultilineDetailItem*)keepDataSeparateItem {
  SettingsMultilineDetailItem* item = [[SettingsMultilineDetailItem alloc]
      initWithType:ItemTypeOptionKeepDataSeparate];
  item.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_KEEP_TITLE);
  if (_isSignedIn) {
    item.detailText = l10n_util::GetNSStringF(
        IDS_IOS_OPTIONS_IMPORT_DATA_KEEP_SUBTITLE_SWITCH,
        base::SysNSStringToUTF16(_fromEmail));
  } else {
    item.detailText = l10n_util::GetNSString(
        IDS_IOS_OPTIONS_IMPORT_DATA_KEEP_SUBTITLE_SIGNIN);
  }
  item.accessoryType = _isSignedIn ? UITableViewCellAccessoryCheckmark
                                   : UITableViewCellAccessoryNone;
  item.accessibilityIdentifier = kImportDataKeepSeparateCellId;
  return item;
}

#pragma mark - UITableViewDelegate

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSection:indexPath.section];
  if (sectionIdentifier != SectionIdentifierOptions)
    return NO;
  return YES;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSection:indexPath.section];

  if (sectionIdentifier == SectionIdentifierOptions) {
    // Store the user choice.
    NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
    _shouldClearData = (itemType == ItemTypeOptionImportData)
                           ? SHOULD_CLEAR_DATA_MERGE_DATA
                           : SHOULD_CLEAR_DATA_CLEAR_DATA;
    [self updateUI];
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Private

// Updates the UI based on the value of |_shouldClearData|.
- (void)updateUI {
  BOOL importDataSelected = _shouldClearData == SHOULD_CLEAR_DATA_MERGE_DATA;
  _importDataItem.accessoryType = importDataSelected
                                      ? UITableViewCellAccessoryCheckmark
                                      : UITableViewCellAccessoryNone;
  _keepDataSeparateItem.accessoryType = importDataSelected
                                            ? UITableViewCellAccessoryNone
                                            : UITableViewCellAccessoryCheckmark;
  [self reconfigureCellsForItems:@[ _importDataItem, _keepDataSeparateItem ]];
}

- (void)didTapContinue {
  [_delegate didChooseClearDataPolicy:self shouldClearData:_shouldClearData];
}

@end
