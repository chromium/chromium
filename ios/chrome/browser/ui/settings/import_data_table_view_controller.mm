// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

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
  ItemTypeOptionKeepDataSeparate,
  ItemTypeOptionImportData,
};

}  // namespace

@implementation ImportDataTableViewController {
  __weak id<ImportDataControllerDelegate> _delegate;
  NSString* _fromEmail;
  NSString* _toEmail;

  // Set to `SHOULD_CLEAR_DATA_USER_CHOICE` to indicate the user did not make
  // any choice to import or clear the data.
  ShouldClearData _shouldClearData;

  SettingsImageDetailTextItem* _keepDataSeparateItem;
  SettingsImageDetailTextItem* _importDataItem;
}

#pragma mark - Initialization

- (instancetype)initWithDelegate:(id<ImportDataControllerDelegate>)delegate
                       fromEmail:(NSString*)fromEmail
                         toEmail:(NSString*)toEmail {
  DCHECK(fromEmail);
  DCHECK(toEmail);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _delegate = delegate;
    _fromEmail = [fromEmail copy];
    _toEmail = [toEmail copy];
    _shouldClearData = SHOULD_CLEAR_DATA_USER_CHOICE;
    self.title = l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_TITLE);
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
  self.navigationItem.rightBarButtonItem.enabled = NO;
  [self loadModel];
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierDisclaimer];
  [model addItem:[self descriptionItem]
      toSectionWithIdentifier:SectionIdentifierDisclaimer];

  _keepDataSeparateItem = [self keepDataSeparateItem];
  _importDataItem = [self importDataItem];
  [model addSectionWithIdentifier:SectionIdentifierOptions];
  [model addItem:_keepDataSeparateItem
      toSectionWithIdentifier:SectionIdentifierOptions];
  [model addItem:_importDataItem
      toSectionWithIdentifier:SectionIdentifierOptions];
}

#pragma mark - Items

- (TableViewItem*)descriptionItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeFooter];
  item.text = l10n_util::GetNSStringF(IDS_IOS_OPTIONS_IMPORT_DATA_HEADER,
                                      base::SysNSStringToUTF16(_fromEmail));
  return item;
}

- (SettingsImageDetailTextItem*)importDataItem {
  DCHECK_EQ(SHOULD_CLEAR_DATA_USER_CHOICE, _shouldClearData);

  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:ItemTypeOptionImportData];
  item.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_IMPORT_TITLE);
  item.detailText =
      l10n_util::GetNSStringF(IDS_IOS_OPTIONS_IMPORT_DATA_IMPORT_SUBTITLE,
                              base::SysNSStringToUTF16(_toEmail));
  item.accessoryType = UITableViewCellAccessoryNone;
  item.accessibilityIdentifier = kImportDataImportCellId;
  return item;
}

- (SettingsImageDetailTextItem*)keepDataSeparateItem {
  DCHECK_EQ(SHOULD_CLEAR_DATA_USER_CHOICE, _shouldClearData);

  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:ItemTypeOptionKeepDataSeparate];
  item.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_IMPORT_DATA_KEEP_SEPARATE_TITLE);
  item.detailText = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_IMPORT_DATA_KEEP_SEPARATE_SUBTITLE);
  item.accessoryType = UITableViewCellAccessoryNone;
  item.accessibilityIdentifier = kImportDataKeepSeparateCellId;
  return item;
}

#pragma mark - UITableViewDelegate

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:indexPath.section];
  if (sectionIdentifier != SectionIdentifierOptions)
    return NO;
  return YES;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:indexPath.section];

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

// Updates the UI based on the value of `_shouldClearData`.
- (void)updateUI {
  switch (_shouldClearData) {
    case SHOULD_CLEAR_DATA_USER_CHOICE:
      self.navigationItem.rightBarButtonItem.enabled = NO;
      _importDataItem.accessoryType = UITableViewCellAccessoryNone;
      _keepDataSeparateItem.accessoryType = UITableViewCellAccessoryNone;
      break;
    case SHOULD_CLEAR_DATA_CLEAR_DATA:
      self.navigationItem.rightBarButtonItem.enabled = YES;
      _importDataItem.accessoryType = UITableViewCellAccessoryNone;
      _keepDataSeparateItem.accessoryType = UITableViewCellAccessoryCheckmark;
      break;
    case SHOULD_CLEAR_DATA_MERGE_DATA:
      self.navigationItem.rightBarButtonItem.enabled = YES;
      _importDataItem.accessoryType = UITableViewCellAccessoryCheckmark;
      _keepDataSeparateItem.accessoryType = UITableViewCellAccessoryNone;
      break;
  }
  [self reconfigureCellsForItems:@[ _keepDataSeparateItem, _importDataItem ]];
}

- (void)didTapContinue {
  DCHECK_NE(SHOULD_CLEAR_DATA_USER_CHOICE, _shouldClearData);
  [_delegate didChooseClearDataPolicy:self shouldClearData:_shouldClearData];
}

@end
