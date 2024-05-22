// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_address_profile_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFields = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSaveAddress = kItemTypeEnumZero,
  ItemTypeMigrateInAccountAddress,
  ItemTypeSaveEmail,
  ItemTypeSavePhone,
  ItemTypeUpdateModalDescription,
  ItemTypeUpdateModalTitle,
  ItemTypeUpdateNameNew,
  ItemTypeUpdateAddressNew,
  ItemTypeUpdateEmailNew,
  ItemTypeUpdatePhoneNew,
  ItemTypeUpdateNameOld,
  ItemTypeUpdateAddressOld,
  ItemTypeUpdateEmailOld,
  ItemTypeUpdatePhoneOld,
  ItemTypeAddressProfileSaveUpdateButton,
  ItemTypeAddressProfileNoThanksButton,
  ItemTypeFooter,
  ItemTypeNotFound
};

const CGFloat kSymbolSize = 16;

// Defines the separator inset value for this table view.
const CGFloat kInfobarSaveAddressProfileSeparatorInset = 54;

}  // namespace

@interface InfobarSaveAddressProfileTableViewController ()

// InfobarSaveAddressProfileModalDelegate for this ViewController.
@property(nonatomic, strong) id<InfobarSaveAddressProfileModalDelegate>
    saveAddressProfileModalDelegate;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// Item for displaying and editing the address.
@property(nonatomic, copy) NSString* address;
// Item for displaying and editing the phone number.
@property(nonatomic, copy) NSString* phoneNumber;
// Item for displaying and editing the email address.
@property(nonatomic, copy) NSString* emailAddress;
// YES if the Address Profile being displayed has been saved.
@property(nonatomic, assign) BOOL currentAddressProfileSaved;
// Yes, if the update address profile modal is to be displayed.
@property(nonatomic, assign) BOOL isUpdateModal;
// Contains the content for the update modal.
@property(nonatomic, copy) NSDictionary* profileDataDiff;
// Description of the update modal.
@property(nonatomic, copy) NSString* updateModalDescription;
// Stores the user email for the currently signed-in account.
@property(nonatomic, copy) NSString* userEmail;
// If YES, denotes that the profile will be added to the Google Account.
@property(nonatomic, assign) BOOL isMigrationToAccount;
// IF YES, for update prompt, the profile belongs to the Google Account.
// For save prompt, denotes that the profile will be saved to Google Account.
@property(nonatomic, assign) BOOL profileAnAccountProfile;
// Description shown in the migration prompt.
@property(nonatomic, copy) NSString* profileDescriptionForMigrationPrompt;

@end

@implementation InfobarSaveAddressProfileTableViewController

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveAddressProfileModalDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _saveAddressProfileModalDelegate = modalDelegate;
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeSaveAutofillAddressProfile];
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.styler.tableViewBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.allowsSelection = NO;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  if (self.isUpdateModal && [self shouldShowOldSection]) {
    [self.tableView
        setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0,
                                           0)];
  } else {
    [self.tableView
        setSeparatorInset:UIEdgeInsetsMake(
                              0, kInfobarSaveAddressProfileSeparatorInset, 0,
                              0)];
  }

  if (!self.isMigrationToAccount || self.currentAddressProfileSaved) {
    // Do not show the cancel button when the migration prompt is presented and
    // the profile is not migrated yet.
    UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(dismissInfobarModal)];
    cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;
    self.navigationItem.leftBarButtonItem = cancelButton;
  }

  if (!self.currentAddressProfileSaved) {
    UIBarButtonItem* editButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemEdit
                             target:self
                             action:@selector(showEditAddressProfileModal)];
    editButton.accessibilityIdentifier = kInfobarSaveAddressModalEditButton;
    self.navigationItem.rightBarButtonItem = editButton;
  }

  self.navigationController.navigationBar.prefersLargeTitles = NO;

  if (self.isMigrationToAccount) {
    self.title = l10n_util::GetNSString(
        IDS_IOS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_TITLE);
  } else if (self.isUpdateModal) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
  } else {
    self.title = l10n_util::GetNSString(
        self.isMigrationToAccount
            ? IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_PROMPT_TITLE
            : IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  }

  [self loadModel];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Presented];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
  [super viewDidDisappear:animated];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.tableView.scrollEnabled =
      self.tableView.contentSize.height > self.view.frame.size.height;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  if (self.isMigrationToAccount) {
    [self loadMigrationToAccountModal];
  } else if (self.isUpdateModal) {
    [self loadUpdateAddressModal];
  } else {
    [self loadSaveAddressModal];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeAddressProfileSaveUpdateButton) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button
               addTarget:self
                  action:@selector(saveAddressProfileButtonWasPressed:)
        forControlEvents:UIControlEventTouchUpInside];
    // Hide the separator line.
    cell.separatorInset =
        UIEdgeInsetsMake(0, 0, 0, self.tableView.bounds.size.width);
  } else if (itemType == ItemTypeAddressProfileNoThanksButton) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button
               addTarget:self
                  action:@selector(noThanksButtonWasPressed:)
        forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeFooter ||
             itemType == ItemTypeUpdateModalDescription ||
             itemType == ItemTypeUpdateModalTitle) {
    // Hide the separator line.
    cell.separatorInset =
        UIEdgeInsetsMake(0, 0, 0, self.tableView.bounds.size.width);
  }
  return cell;
}

#pragma mark - InfobarSaveAddressProfileModalConsumer

- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs {
  self.address = prefs[kAddressPrefKey];
  self.phoneNumber = prefs[kPhonePrefKey];
  self.emailAddress = prefs[kEmailPrefKey];
  self.currentAddressProfileSaved =
      [prefs[kCurrentAddressProfileSavedPrefKey] boolValue];
  self.isUpdateModal = [prefs[kIsUpdateModalPrefKey] boolValue];
  self.profileDataDiff = prefs[kProfileDataDiffKey];
  self.updateModalDescription = prefs[kUpdateModalDescriptionKey];
  self.isMigrationToAccount = [prefs[kIsMigrationToAccountKey] boolValue];
  self.userEmail = prefs[kUserEmailKey];
  self.profileAnAccountProfile =
      [prefs[kIsProfileAnAccountProfileKey] boolValue];
  self.profileDescriptionForMigrationPrompt =
      prefs[kProfileDescriptionForMigrationPromptKey];
  [self.tableView reloadData];
}

#pragma mark - Actions

- (void)saveAddressProfileButtonWasPressed:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];
  [self.saveAddressProfileModalDelegate modalInfobarButtonWasAccepted:self];
}

- (void)dismissInfobarModal {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.saveAddressProfileModalDelegate dismissInfobarModal:self];
}

- (void)showEditAddressProfileModal {
  if (base::FeatureList::IsEnabled(
          kAutofillDynamicallyLoadsFieldsForAddressInput)) {
    [self.saveAddressProfileModalDelegate dismissInfobarModal:self];
  }
  [self.saveAddressProfileModalDelegate showEditView];
}

- (void)noThanksButtonWasPressed:(UIButton*)sender {
  [self.saveAddressProfileModalDelegate noThanksButtonWasPressed];
}

#pragma mark - Private Methods

- (void)loadUpdateAddressModal {
  DCHECK([self.profileDataDiff count] > 0);

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierFields];
  [model addItem:[self updateModalDescriptionItem]
      toSectionWithIdentifier:SectionIdentifierFields];

  BOOL showOld = [self shouldShowOldSection];

  if (showOld) {
    TableViewTextItem* newTitleItem = [self
        titleWithTextItem:
            l10n_util::GetNSString(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_NEW_VALUES_SECTION_LABEL)];
    [model addItem:newTitleItem
        toSectionWithIdentifier:SectionIdentifierFields];
  }

  for (NSNumber* type in self.profileDataDiff) {
    if ([self.profileDataDiff[type][0] length] > 0) {
      ItemType itemType =
          [self itemTypeForUpdateModelFromAutofillType:static_cast<
                                                           autofill::FieldType>(
                                                           [type intValue])
                                                   old:NO];

      SettingsImageDetailTextItem* newItem =
          [self detailItemWithType:itemType
                              text:self.profileDataDiff[type][0]
                            symbol:[self symbolForItemType:itemType]
              imageTintColorIsGrey:NO];
      [model addItem:newItem toSectionWithIdentifier:SectionIdentifierFields];
    }
  }

  if (showOld) {
    TableViewTextItem* oldTitleItem = [self
        titleWithTextItem:
            l10n_util::GetNSString(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OLD_VALUES_SECTION_LABEL)];
    [model addItem:oldTitleItem
        toSectionWithIdentifier:SectionIdentifierFields];
    for (NSNumber* type in self.profileDataDiff) {
      if ([self.profileDataDiff[type][1] length] > 0) {
        ItemType itemType = [self
            itemTypeForUpdateModelFromAutofillType:static_cast<
                                                       autofill::FieldType>(
                                                       [type intValue])
                                               old:YES];
        SettingsImageDetailTextItem* oldItem =
            [self detailItemWithType:itemType
                                text:self.profileDataDiff[type][1]
                              symbol:[self symbolForItemType:itemType]
                imageTintColorIsGrey:YES];
        [model addItem:oldItem toSectionWithIdentifier:SectionIdentifierFields];
      }
    }
  }

  if (self.profileAnAccountProfile) {
    [model addItem:[self updateFooterItem]
        toSectionWithIdentifier:SectionIdentifierFields];
  }

  [model addItem:[self saveUpdateButton]
      toSectionWithIdentifier:SectionIdentifierFields];
}

- (void)loadSaveAddressModal {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierFields];

  SettingsImageDetailTextItem* addressItem =
      [self detailItemTypeForSaveModal:ItemTypeSaveAddress
                              withText:self.address];
  [model addItem:addressItem toSectionWithIdentifier:SectionIdentifierFields];

  if ([self.emailAddress length]) {
    SettingsImageDetailTextItem* emailItem =
        [self detailItemTypeForSaveModal:ItemTypeSaveEmail
                                withText:self.emailAddress];
    [model addItem:emailItem toSectionWithIdentifier:SectionIdentifierFields];
  }

  if ([self.phoneNumber length]) {
    SettingsImageDetailTextItem* phoneItem =
        [self detailItemTypeForSaveModal:ItemTypeSavePhone
                                withText:self.phoneNumber];
    [model addItem:phoneItem toSectionWithIdentifier:SectionIdentifierFields];
  }

  if (self.isMigrationToAccount || self.profileAnAccountProfile) {
    [model addItem:[self saveFooterItem]
        toSectionWithIdentifier:SectionIdentifierFields];
  }

  [model addItem:[self saveUpdateButton]
      toSectionWithIdentifier:SectionIdentifierFields];
}

- (void)loadMigrationToAccountModal {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierFields];

  [model addItem:[self migrationPromptFooterItem]
      toSectionWithIdentifier:SectionIdentifierFields];

  SettingsImageDetailTextItem* addressItem = [self
        detailItemWithType:ItemTypeMigrateInAccountAddress
                      text:self.profileDescriptionForMigrationPrompt
                    symbol:
                        [self symbolForItemType:ItemTypeMigrateInAccountAddress]
      imageTintColorIsGrey:YES];
  [model addItem:addressItem toSectionWithIdentifier:SectionIdentifierFields];

  [model addItem:[self saveUpdateButton]
      toSectionWithIdentifier:SectionIdentifierFields];

  if (!self.currentAddressProfileSaved) {
    [model addItem:[self noThanksButton]
        toSectionWithIdentifier:SectionIdentifierFields];
  }
}

- (TableViewTextButtonItem*)saveUpdateButton {
  TableViewTextButtonItem* saveUpdateButton = [[TableViewTextButtonItem alloc]
      initWithType:ItemTypeAddressProfileSaveUpdateButton];
  saveUpdateButton.textAlignment = NSTextAlignmentNatural;

  if (self.isMigrationToAccount) {
    saveUpdateButton.buttonText = l10n_util::GetNSString(
        IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL);
  } else if (self.isUpdateModal) {
    saveUpdateButton.buttonText = l10n_util::GetNSString(
        IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  } else {
    saveUpdateButton.buttonText = l10n_util::GetNSString(
        IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  }

  saveUpdateButton.enabled = !self.currentAddressProfileSaved;
  saveUpdateButton.disableButtonIntrinsicWidth = YES;
  return saveUpdateButton;
}

- (TableViewTextButtonItem*)noThanksButton {
  TableViewTextButtonItem* noThanksButton = [[TableViewTextButtonItem alloc]
      initWithType:ItemTypeAddressProfileNoThanksButton];
  noThanksButton.textAlignment = NSTextAlignmentNatural;
  noThanksButton.buttonBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  noThanksButton.buttonTextColor = [UIColor colorNamed:kBlueColor];
  noThanksButton.buttonText = l10n_util::GetNSString(
      IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_CANCEL_BUTTON_LABEL);
  return noThanksButton;
}

- (TableViewTextItem*)titleWithTextItem:(NSString*)text {
  TableViewTextItem* titleItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeUpdateModalTitle];
  titleItem.text = text;
  titleItem.textFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  return titleItem;
}

- (TableViewTextItem*)updateModalDescriptionItem {
  TableViewTextItem* descriptionItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeUpdateModalDescription];
  descriptionItem.text = self.updateModalDescription;
  descriptionItem.textFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  descriptionItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return descriptionItem;
}

// Return symbol based on the `itemType`.
- (UIImage*)symbolForItemType:(ItemType)itemType {
  switch (itemType) {
    case ItemTypeUpdateNameNew:
    case ItemTypeUpdateNameOld:
      return DefaultSymbolTemplateWithPointSize(kPersonFillSymbol, kSymbolSize);
    case ItemTypeSaveAddress:
    case ItemTypeUpdateAddressNew:
    case ItemTypeUpdateAddressOld:
      return CustomSymbolTemplateWithPointSize(kLocationSymbol, kSymbolSize);
    case ItemTypeSaveEmail:
    case ItemTypeUpdateEmailNew:
    case ItemTypeUpdateEmailOld:
      return DefaultSymbolTemplateWithPointSize(kMailFillSymbol, kSymbolSize);
    case ItemTypeSavePhone:
    case ItemTypeUpdatePhoneNew:
    case ItemTypeUpdatePhoneOld:
      return DefaultSymbolTemplateWithPointSize(kPhoneFillSymbol, kSymbolSize);
    case ItemTypeMigrateInAccountAddress:
      return CustomSymbolTemplateWithPointSize(kLocationSymbol, kSymbolSize);
    default:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return nil;
}

// Returns the item type corresponding to the `type` for the update modal view.
- (ItemType)itemTypeForUpdateModelFromAutofillType:(autofill::FieldType)type
                                               old:(BOOL)old {
  switch (type) {
    case autofill::ADDRESS_HOME_STREET_ADDRESS:
    case autofill::ADDRESS_HOME_ADDRESS:
      return old ? ItemTypeUpdateAddressOld : ItemTypeUpdateAddressNew;
    case autofill::EMAIL_ADDRESS:
      return old ? ItemTypeUpdateEmailOld : ItemTypeUpdateEmailNew;
    case autofill::PHONE_HOME_WHOLE_NUMBER:
      return old ? ItemTypeUpdatePhoneOld : ItemTypeUpdatePhoneNew;
    case autofill::NAME_FULL:
      return old ? ItemTypeUpdateNameOld : ItemTypeUpdateNameNew;
    default:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return ItemTypeNotFound;
}

// Returns YES if the old section is shown in the update modal.
- (BOOL)shouldShowOldSection {
  // Determines whether the old section is to be shown or not.
  for (NSNumber* type in self.profileDataDiff) {
    if ([self.profileDataDiff[type][1] length] > 0) {
      return YES;
    }
  }

  return NO;
}

#pragma mark - Item Constructors

// Returns a `SettingsImageDetailTextItem` for the fields to be shown in the
// save address modal.
- (SettingsImageDetailTextItem*)detailItemTypeForSaveModal:(ItemType)itemType
                                                  withText:(NSString*)text {
  return [self detailItemWithType:itemType
                             text:text
                           symbol:[self symbolForItemType:itemType]
             imageTintColorIsGrey:YES];
}

- (SettingsImageDetailTextItem*)detailItemWithType:(NSInteger)type
                                              text:(NSString*)text
                                            symbol:(UIImage*)symbol
                              imageTintColorIsGrey:(BOOL)imageTintColorIsGrey {
  SettingsImageDetailTextItem* detailItem =
      [[SettingsImageDetailTextItem alloc] initWithType:type];

  detailItem.text = text;
  detailItem.alignImageWithFirstLineOfText = YES;
  if (symbol) {
    detailItem.image = symbol;
    if (imageTintColorIsGrey) {
      detailItem.imageViewTintColor = [UIColor colorNamed:kGrey400Color];
    } else {
      detailItem.imageViewTintColor = [UIColor colorNamed:kBlueColor];
    }
  }

  return detailItem;
}

- (TableViewTextItem*)saveFooterItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeFooter];
  int footerTextId = self.currentAddressProfileSaved
                         ? IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT
                         : IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER;
  CHECK([self.userEmail length] > 0);
  item.text = l10n_util::GetNSStringF(footerTextId,
                                      base::SysNSStringToUTF16(self.userEmail));
  item.textFont = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  item.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return item;
}

- (TableViewTextItem*)updateFooterItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeFooter];
  CHECK([self.userEmail length] > 0);
  item.text = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
      base::SysNSStringToUTF16(self.userEmail));
  item.textFont = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  item.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return item;
}

- (TableViewTextItem*)migrationPromptFooterItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeFooter];
  int footerTextId = self.currentAddressProfileSaved
                         ? IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT
                         : IDS_IOS_AUTOFILL_ADDRESS_MIGRATE_IN_ACCOUNT_FOOTER;
  CHECK([self.userEmail length] > 0);
  item.text = l10n_util::GetNSStringF(footerTextId,
                                      base::SysNSStringToUTF16(self.userEmail));
  item.textFont = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  item.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return item;
}

@end
