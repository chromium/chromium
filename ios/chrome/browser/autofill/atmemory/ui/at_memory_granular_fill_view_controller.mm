// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_cell_content_configuration.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_mutator.h"
#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Section identifiers for the diffable data source.
enum SectionIdentifier {
  // Section displaying granular fill items.
  kGranularFillItemsSection = kSectionIdentifierEnumZero,
  // Section displaying "Manage enhanced autofill" action link.
  kManageSection,
};

// Item identifiers displayed in the granular fill table view.
enum ItemIdentifier {
  // Row displaying an attribute chip to fill content.
  kGranularFillItem = kItemTypeEnumZero,
  // Row displaying the "Manage enhanced autofill" action link.
  kManageEnhancedAutofillItem,
};

}  // namespace

@implementation AtMemoryGranularFillViewController {
  // Diffable data source for the table view.
  UITableViewDiffableDataSource<NSNumber*, id>* _dataSource;
  // List of dynamic granular fill items to display.
  NSArray<AtMemoryGranularFillItem*>* _granularFillItems;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.allowsSelection = YES;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(handleCancelButton)];
  cancelButton.accessibilityIdentifier =
      kAtMemoryCloseButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = cancelButton;

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          id itemIdentifier) {
             return [weakSelf cellForTableView:tableView
                                     indexPath:indexPath
                                itemIdentifier:itemIdentifier];
           }];

  [TableViewCellContentConfiguration registerCellForTableView:self.tableView];
  RegisterTableViewHeaderFooter<TableViewLinkHeaderFooterView>(self.tableView);

  [self createNewSnapshot];
}

#pragma mark - Actions

- (void)handleCancelButton {
  [self.atMemoryHandler dismissAtMemory];
}

#pragma mark - AtMemoryGranularFillConsumer

- (void)setTitle:(NSString*)title {
  [super setTitle:title];
}

- (void)setGranularFillItems:(NSArray<AtMemoryGranularFillItem*>*)items {
  _granularFillItems = [items copy];
  if (_dataSource) {
    [self createNewSnapshot];
  }
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].unsignedIntegerValue);
  if (sectionIdentifier == kGranularFillItemsSection) {
    TableViewLinkHeaderFooterView* footer =
        DequeueTableViewHeaderFooter<TableViewLinkHeaderFooterView>(tableView);
    [footer setText:l10n_util::GetNSString(IDS_AUTOFILL_AI_SUGGESTED_BY_GEMINI)
          withColor:[UIColor colorNamed:kTextSecondaryColor]];
    return footer;
  }
  return nil;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].unsignedIntegerValue);
  if (sectionIdentifier == kGranularFillItemsSection) {
    return UITableViewAutomaticDimension;
  }
  return 0.0;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return 0.0;
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  if (!_dataSource) {
    return NO;
  }
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:indexPath.section]
          .unsignedIntegerValue);
  return sectionIdentifier == kManageSection;
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (!_dataSource) {
    return nil;
  }
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:indexPath.section]
          .unsignedIntegerValue);
  if (sectionIdentifier == kGranularFillItemsSection) {
    return nil;
  }
  return indexPath;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  id itemIdentifier = [_dataSource itemIdentifierForIndexPath:indexPath];
  if ([itemIdentifier isEqual:@(kManageEnhancedAutofillItem)]) {
    // TODO(crbug.com/522340351): Update after confirming with UX.
    [self.atMemoryHandler openAutofillSettings];
  }
}

#pragma mark - Private

// Creates the diffable data source snapshot with the granular fill section
// and manage enhanced autofill section.
- (void)createNewSnapshot {
  NSDiffableDataSourceSnapshot<NSNumber*, id>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  [snapshot appendSectionsWithIdentifiers:@[
    @(kGranularFillItemsSection),
    @(kManageSection),
  ]];

  if (_granularFillItems.count > 0) {
    [snapshot appendItemsWithIdentifiers:_granularFillItems
               intoSectionWithIdentifier:@(kGranularFillItemsSection)];
  }

  [snapshot appendItemsWithIdentifiers:@[ @(kManageEnhancedAutofillItem) ]
             intoSectionWithIdentifier:@(kManageSection)];

  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Returns a configured table view cell for the given item identifier.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(id)itemIdentifier {
  if ([itemIdentifier isEqual:@(kManageEnhancedAutofillItem)]) {
    return [self manageEnhancedAutofillCellForTableView:tableView];
  }

  if ([itemIdentifier isKindOfClass:[AtMemoryGranularFillItem class]]) {
    return [self
        granularFillCellForTableView:tableView
                                item:(AtMemoryGranularFillItem*)itemIdentifier];
  }

  return nil;
}

// Returns a cell configured for the manage enhanced autofill action item.
- (UITableViewCell*)manageEnhancedAutofillCellForTableView:
    (UITableView*)tableView {
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title =
      l10n_util::GetNSString(IDS_AUTOFILL_MANAGE_ENHANCED_AUTOFILL);
  configuration.titleColor = [UIColor colorNamed:kBlueColor];
  cell.accessibilityIdentifier =
      kAtMemoryManageEnhancedAutofillItemAccessibilityIdentifier;
  cell.contentConfiguration = configuration;
  cell.accessoryType = UITableViewCellAccessoryNone;
  cell.selectionStyle = UITableViewCellSelectionStyleDefault;
  return cell;
}

// Returns a cell configured for a granular fill item chip.
- (UITableViewCell*)granularFillCellForTableView:(UITableView*)tableView
                                            item:(AtMemoryGranularFillItem*)
                                                     item {
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  AtMemoryGranularFillCellContentConfiguration* configuration =
      [AtMemoryGranularFillCellContentConfiguration cellConfiguration];
  configuration.attributeName = item.attributeName;
  configuration.attributeValue = item.attributeValue;
  __weak __typeof(self) weakSelf = self;
  configuration.selectionHandler = ^(NSString* content) {
    [weakSelf.mutator didSelectGranularFillItem:item];
  };
  cell.contentConfiguration = configuration;
  cell.accessibilityIdentifier =
      GetAtMemoryGranularFillCellAccessibilityIdentifier(item.attributeName);
  cell.accessoryType = UITableViewCellAccessoryNone;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  return cell;
}

@end
