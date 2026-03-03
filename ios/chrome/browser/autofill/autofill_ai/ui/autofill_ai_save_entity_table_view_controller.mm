// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_mutator.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/autofill_edit_profile_button_footer_item.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierNewEntity = 0,
  SectionIdentifierOldEntity,
  SectionIdentifierFooter,
  SectionIdentifierActions,
  SectionCount,
};

// Creates an array of `TableViewTextEditItem` objects from an `entity`.
NSArray<TableViewTextEditItem*>* CreateItemsFromEntity(
    const autofill::EntityInstance& entity) {
  NSMutableArray<TableViewTextEditItem*>* items = [[NSMutableArray alloc] init];
  std::string locale =
      base::SysNSStringToUTF8([[NSLocale currentLocale] localeIdentifier]);

  for (const auto& attribute : entity.attributes()) {
    TableViewTextEditItem* item = [[TableViewTextEditItem alloc] init];
    item.fieldNameLabelText =
        base::SysUTF16ToNSString(attribute.type().GetNameForI18n());
    item.textFieldValue =
        base::SysUTF16ToNSString(attribute.GetCompleteInfo(locale));
    item.textFieldEnabled = NO;
    item.hideIcon = YES;
    [items addObject:item];
  }
  return items;
}

void RegisterCells(UITableView* table_view) {
  RegisterTableViewCell<TableViewTextEditCell>(table_view);
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(table_view);
  RegisterTableViewHeaderFooter<AutofillEditProfileButtonFooterCell>(
      table_view);
  RegisterTableViewHeaderFooter<TableViewLinkHeaderFooterView>(table_view);
}

void AddEntity(
    NSDiffableDataSourceSnapshot<NSNumber*, TableViewItem*>* snapshot,
    const autofill::EntityInstance& entity,
    SectionIdentifier sectionIdentifier) {
  [snapshot appendSectionsWithIdentifiers:@[
    @(sectionIdentifier),
  ]];
  NSArray<TableViewItem*>* items = CreateItemsFromEntity(entity);
  [snapshot appendItemsWithIdentifiers:items
             intoSectionWithIdentifier:@(sectionIdentifier)];
}

TableViewTextHeaderFooterView* GetHeaderView(UITableView* table_view,
                                             int message_id) {
  TableViewTextHeaderFooterView* header =
      DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(table_view);
  [header setTitle:l10n_util::GetNSString(message_id)];
  return header;
}

}  // namespace

@interface AutofillAISaveEntityTableViewController () <
    AutofillEditProfileButtonFooterDelegate>
@end

@implementation AutofillAISaveEntityTableViewController {
  // New entity to save.
  std::optional<autofill::EntityInstance> _newEntity;

  // Old entity to compare against.
  std::optional<autofill::EntityInstance> _oldEntity;

  // User email to display in the footer.
  std::u16string _userEmail;

  // Diffable data source for the table view.
  UITableViewDiffableDataSource<NSNumber*, TableViewItem*>* _dataSource;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier = kAutofillAISaveEntityTableViewId;

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(handleCancelButton)];
  cancelButton.accessibilityIdentifier = kAutofillAISaveEntityCancelButtonId;
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  RegisterCells(self.tableView);

  [self loadModel];
}

- (void)loadModel {
  if (!_newEntity.has_value()) {
    return;
  }

  [self setTitle:base::SysUTF16ToNSString(_newEntity->type().GetNameForI18n())];

  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          TableViewItem* item) {
             TableViewTextEditCell* cell =
                 DequeueTableViewCell<TableViewTextEditCell>(tableView);
             TableViewTextEditItem* textEditItem =
                 base::apple::ObjCCastStrict<TableViewTextEditItem>(item);
             cell.textLabel.text = textEditItem.fieldNameLabelText;
             cell.textField.text = textEditItem.textFieldValue;
             cell.textField.enabled = textEditItem.isTextFieldEnabled;
             [cell setIcon:textEditItem.hideIcon
                               ? TableViewTextEditItemIconTypeNone
                               : TableViewTextEditItemIconTypeEdit];
             return cell;
           }];

  NSDiffableDataSourceSnapshot<NSNumber*, TableViewItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  AddEntity(snapshot, *_newEntity, SectionIdentifierNewEntity);

  if (_oldEntity.has_value()) {
    AddEntity(snapshot, *_oldEntity, SectionIdentifierOldEntity);
  }

  [snapshot appendSectionsWithIdentifiers:@[
    @(SectionIdentifierFooter),
    @(SectionIdentifierActions),
  ]];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - AutofillAISaveEntityConsumer

- (void)setNewEntity:(autofill::EntityInstance)newEntity
           oldEntity:(std::optional<autofill::EntityInstance>)oldEntity
           userEmail:(const std::u16string&)userEmail {
  _newEntity = std::move(newEntity);
  _oldEntity = std::move(oldEntity);
  _userEmail = userEmail;
  if (self.viewLoaded) {
    [self loadModel];
  }
}

#pragma mark - UITableView header and footer overrides

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  if (!_oldEntity.has_value()) {
    return nil;
  }

  SectionIdentifier sectionIdentifier =
      [self sectionIdentifierForSection:section];

  if (sectionIdentifier == SectionIdentifierNewEntity) {
    return GetHeaderView(
        tableView, IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_NEW_VALUES_SECTION_LABEL);
  } else if (sectionIdentifier == SectionIdentifierOldEntity) {
    return GetHeaderView(
        tableView, IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OLD_VALUES_SECTION_LABEL);
  }

  return nil;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier =
      [self sectionIdentifierForSection:section];

  if (sectionIdentifier == SectionIdentifierFooter) {
    TableViewLinkHeaderFooterView* footer =
        DequeueTableViewHeaderFooter<TableViewLinkHeaderFooterView>(tableView);
    [footer setText:[self footerText]
          withColor:[UIColor colorNamed:kTextSecondaryColor]];
    return footer;
  }

  if (sectionIdentifier == SectionIdentifierActions) {
    AutofillEditProfileButtonFooterCell* footer =
        DequeueTableViewHeaderFooter<AutofillEditProfileButtonFooterCell>(
            tableView);
    footer.delegate = self;
    [footer.button setTitle:[self acceptButtonText]
                   forState:UIControlStateNormal];
    footer.button.enabled = YES;
    return footer;
  }

  return nil;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier =
      [self sectionIdentifierForSection:section];

  if ((sectionIdentifier == SectionIdentifierNewEntity ||
       sectionIdentifier == SectionIdentifierOldEntity) &&
      _oldEntity.has_value()) {
    return UITableViewAutomaticDimension;
  }

  return 0;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier =
      [self sectionIdentifierForSection:section];

  if (sectionIdentifier == SectionIdentifierFooter ||
      sectionIdentifier == SectionIdentifierActions) {
    return UITableViewAutomaticDimension;
  }

  return 0;
}

#pragma mark - Actions

- (void)handleCancelButton {
  [self.mutator cancelSaving];
  [self.autofillHandler dismissSaveEntityDialog];
}

#pragma mark - AutofillEditProfileButtonFooterDelegate

- (void)didTapButton {
  [self.mutator acceptSaving];
  [self.autofillHandler dismissSaveEntityDialog];
}

#pragma mark - Private

- (SectionIdentifier)sectionIdentifierForSection:(NSInteger)section {
  return static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
}

- (BOOL)isSaveToWallet {
  return _newEntity.has_value() &&
         _newEntity->record_type() ==
             autofill::EntityInstance::RecordType::kServerWallet;
}

- (NSString*)footerText {
  if ([self isSaveToWallet]) {
    return l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_AI_FOOTER_SAVE_TO_WALLET,
                                   _userEmail);
  } else {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FOOTER_SAVE_TO_DEVICE);
  }
}

- (NSString*)acceptButtonText {
  return l10n_util::GetNSString(
      _oldEntity.has_value()
          ? IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL
          : IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
}

@end
