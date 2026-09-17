// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/filling/field_filling_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/model/message/autofill_legal_message_line.h"
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

constexpr CGFloat kSaveFormTopPadding = 16.0;
constexpr CGFloat kUpdateFormSectionSpacing = 32.0;

// 16pt padding between attributes and the footer text.
constexpr CGFloat kFooterTopExtraPadding = 16.0;

// Blank line separating the storage notice from the disclosure legal messages.
NSString* const kStorageNoticeSeparator = @"\n\n";

// Line break separating consecutive disclosure legal messages.
NSString* const kDisclosureLegalMessageSeparator = @"\n";

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

// Returns the text of `legal_message` with each valid link wrapped in link tags
// by `autofill::WrapInLinkTags()`, appending the corresponding URLs to `urls`
// in the order their tags appear in the returned string. Ranges that are
// invalid or that overlap a previous one, as well as links with an invalid URL,
// are emitted as plain text so that the number of tags always matches the
// number of appended URLs.
NSString* TextForDisclosureLegalMessageAppendingURLsTo(
    AutofillLegalMessageLine* legal_message,
    NSMutableArray<CrURL*>* urls) {
  NSString* lineText = legal_message.messageText;
  NSArray<NSValue*>* linkRanges = legal_message.linkRanges;
  const std::vector<GURL>& linkURLs = legal_message.linkURLs;

  NSMutableArray<NSString*>* fragments = [NSMutableArray array];
  NSUInteger currentIndex = 0;
  for (NSUInteger i = 0; i < linkRanges.count && i < linkURLs.size(); ++i) {
    NSRange range = [linkRanges[i] rangeValue];
    if (range.location == NSNotFound || range.location < currentIndex ||
        NSMaxRange(range) > lineText.length) {
      continue;
    }

    [fragments
        addObject:[lineText substringWithRange:NSMakeRange(currentIndex,
                                                           range.location -
                                                               currentIndex)]];

    NSString* linkText = [lineText substringWithRange:range];
    const GURL& url = linkURLs[i];
    if (url.is_valid()) {
      [fragments addObject:autofill::WrapInLinkTags(linkText)];
      [urls addObject:[[CrURL alloc] initWithGURL:url]];
    } else {
      [fragments addObject:linkText];
    }

    currentIndex = NSMaxRange(range);
  }

  [fragments addObject:[lineText substringFromIndex:currentIndex]];
  return [fragments componentsJoinedByString:@""];
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

  // Legal message lines to display in the footer.
  NSArray<AutofillLegalMessageLine*>* _legalMessages;

  // Cached footer text and URLs computed when the model updates.
  NSString* _cachedFooterText;
  NSArray<CrURL*>* _cachedFooterURLs;

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
  NSString* title = [self isUpdateDialog]
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

  if ([self isUpdateDialog]) {
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
  [self updateCachedFooter];
  if (self.viewLoaded) {
    [self loadModel];
  }
}

- (void)setLegalMessages:(NSArray<AutofillLegalMessageLine*>*)legalMessages {
  _legalMessages = legalMessages;
  [self updateCachedFooter];
  if (self.viewLoaded && _dataSource) {
    NSDiffableDataSourceSnapshot<NSNumber*, TableViewItem*>* snapshot =
        _dataSource.snapshot;
    [snapshot reloadSectionsWithIdentifiers:@[ @(SectionIdentifierFooter) ]];
    [_dataSource applySnapshot:snapshot animatingDifferences:NO];
  }
}

#pragma mark - UITableView header and footer overrides

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  if (![self isUpdateDialog]) {
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
    footer.accessibilityIdentifier =
        _legalMessages.count > 0 ? kAutofillAISaveEntityLegalDisclosureId : nil;
    [self configureFooterView:footer];
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
      [self isUpdateDialog]) {
    return UITableViewAutomaticDimension;
  }

  if (![self isUpdateDialog] &&
      sectionIdentifier == SectionIdentifierNewEntity) {
    return kSaveFormTopPadding;
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

  if (sectionIdentifier == SectionIdentifierOldEntity) {
    return kFooterTopExtraPadding;
  }

  if (sectionIdentifier == SectionIdentifierNewEntity) {
    if ([self isUpdateDialog]) {
      return kUpdateFormSectionSpacing;
    } else {
      return kFooterTopExtraPadding;
    }
  }

  return 0;
}

#pragma mark - Private

- (SectionIdentifier)sectionIdentifierForSection:(NSInteger)section {
  return static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
}

// Returns if the current dialog is the `Update` dialog. If it returns `NO`, the
// current dialog is the `Save` dialog.
- (BOOL)isUpdateDialog {
  return _oldEntity.has_value();
}

- (BOOL)isSaveToWallet {
  return _newEntity.has_value() &&
         _newEntity->record_type() ==
             autofill::EntityInstance::RecordType::kServerWallet;
}

// Returns the storage notice text shown at the top of the footer, which
// explains where the entity is saved (this device or Google Wallet). This text
// is authored by Chrome, as opposed to the disclosure legal messages below it,
// which come from the server. Appends the URL it links to, if any, to `urls`.
- (NSString*)textForStorageNoticeAppendingURLsTo:(NSMutableArray<CrURL*>*)urls {
  if (![self isSaveToWallet]) {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FOOTER_SAVE_TO_DEVICE);
  }

  NSString* email = base::SysUTF16ToNSString(_userEmail);
  NSString* storageNoticeText =
      [self isUpdateDialog]
          ? autofill::GetUpdateEntitySavedInWalletFooterText(email)
          : autofill::GetSaveEntityToWalletFooterText(email);
  GURL url = [self isUpdateDialog] ? autofill::GetGoogleWalletPassesURL()
                                   : autofill::GetManageYourInfoURL();
  [urls addObject:[[CrURL alloc] initWithGURL:url]];
  return storageNoticeText;
}

// Computes and caches the footer text and URLs based on current model data.
- (void)updateCachedFooter {
  // `urls` must be filled in the same order the links appear in the text, so
  // the storage notice is built before the disclosure legal messages.
  NSMutableArray<CrURL*>* urls = [NSMutableArray array];
  NSString* storageNoticeText = [self textForStorageNoticeAppendingURLsTo:urls];

  NSMutableArray<NSString*>* disclosureLegalMessageTexts =
      [NSMutableArray array];
  for (AutofillLegalMessageLine* disclosureLegalMessage in _legalMessages) {
    [disclosureLegalMessageTexts
        addObject:TextForDisclosureLegalMessageAppendingURLsTo(
                      disclosureLegalMessage, urls)];
  }

  // The disclosure legal messages form a single block, separated from the
  // storage notice by a blank line.
  NSString* text = storageNoticeText;
  if (disclosureLegalMessageTexts.count > 0) {
    text = [NSString
        stringWithFormat:
            @"%@%@%@", storageNoticeText, kStorageNoticeSeparator,
            [disclosureLegalMessageTexts
                componentsJoinedByString:kDisclosureLegalMessageSeparator]];
  }

  _cachedFooterText = text;
  _cachedFooterURLs = urls;
}

// Configures the footer view with the cached storage notice, disclosure legal
// messages, and URLs.
- (void)configureFooterView:(TableViewLinkHeaderFooterView*)footer {
  if (!_cachedFooterText) {
    [self updateCachedFooter];
  }
  footer.urls = _cachedFooterURLs;
  [footer setText:_cachedFooterText
        withColor:[UIColor colorNamed:kTextSecondaryColor]];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.delegate didTapLinkWithURL:URL];
}

@end
