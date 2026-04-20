// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/filling/field_filling_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_date_util.h"
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
  SectionCount,
};

// Creates an array of `TableViewTextEditItem` objects from an `entity`.
NSArray<TableViewTextEditItem*>* CreateItemsFromEntity(
    const autofill::EntityInstance& entity) {
  NSMutableArray<TableViewTextEditItem*>* items = [[NSMutableArray alloc] init];
  std::string locale =
      base::SysNSStringToUTF8([[NSLocale currentLocale] localeIdentifier]);

  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  dateFormatter.dateStyle = NSDateFormatterMediumStyle;
  dateFormatter.timeStyle = NSDateFormatterNoStyle;
  dateFormatter.locale =
      [NSLocale localeWithLocaleIdentifier:base::SysUTF8ToNSString(locale)];

  for (const auto& attribute : entity.attributes()) {
    TableViewTextEditItem* item = [[TableViewTextEditItem alloc] init];
    item.fieldNameLabelText =
        autofill::DisplayNameForAutofillAiAttributeType(attribute.type());

    std::u16string value;
    if (attribute.type().data_type() ==
        autofill::AttributeType::DataType::kDate) {
      NSDate* dateValue = NSDateFromAttributeInstance(attribute);
      value =
          base::SysNSStringToUTF16([dateFormatter stringFromDate:dateValue]);
    } else {
      value = attribute.GetCompleteInfo(locale);
    }

    if (attribute.masked()) {
      // If the attribute is masked, the obfuscated value is shown.
      value = autofill::GetObfuscatedValue(value, /*visible_suffix_length=*/4);
    }
    item.textFieldValue = base::SysUTF16ToNSString(value);
    item.textFieldEnabled = NO;
    item.hideIcon = YES;
    [items addObject:item];
  }
  return items;
}

void RegisterCells(UITableView* table_view) {
  RegisterTableViewCell<TableViewTextEditCell>(table_view);
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(table_view);
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
    TableViewLinkHeaderFooterItemDelegate>
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
  self.tableView.allowsSelection = NO;

  RegisterCells(self.tableView);

  [self loadModel];
}

- (void)loadModel {
  if (!_newEntity.has_value()) {
    return;
  }

  autofill::EntityTypeName typeName = _newEntity->type().name();
  NSString* title = _oldEntity.has_value()
                        ? autofill::GetDialogTitleForUpdateEntity(typeName)
                        : autofill::GetDialogTitleForSaveEntity(typeName);
  [self setTitle:title];

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
  ]];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - Public Methods

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
    footer.delegate = self;
    if ([self isSaveToWallet]) {
      GURL url = _oldEntity.has_value() ? autofill::GetGoogleWalletPassesURL()
                                        : autofill::GetManageYourInfoURL();
      footer.urls = @[ [[CrURL alloc] initWithGURL:url] ];
    }
    [footer setText:[self footerText]
          withColor:[UIColor colorNamed:kTextSecondaryColor]];
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

  if (sectionIdentifier == SectionIdentifierFooter) {
    return UITableViewAutomaticDimension;
  }

  return 0;
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
    if (_oldEntity.has_value()) {
      return autofill::GetUpdateEntitySavedInWalletFooterText(
          base::SysUTF16ToNSString(_userEmail));
    } else {
      return autofill::GetSaveEntityToWalletFooterText(
          base::SysUTF16ToNSString(_userEmail));
    }
  } else {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FOOTER_SAVE_TO_DEVICE);
  }
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.delegate didTapLinkWithURL:URL];
}

@end
