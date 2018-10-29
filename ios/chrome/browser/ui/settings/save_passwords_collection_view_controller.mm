// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/save_passwords_collection_view_controller.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"

#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/google/core/common/google_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/save_passwords_consumer.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/alpha_animated_collection_view_flow_layout.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_search_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/ui/settings/password_details_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/password_details_collection_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password_exporter.h"
#import "ios/chrome/browser/ui/settings/reauthentication_module.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMessage = kSectionIdentifierEnumZero,
  SectionIdentifierSearchPasswordsBox,
  SectionIdentifierSavePasswordsSwitch,
  SectionIdentifierSavedPasswords,
  SectionIdentifierBlacklist,
  SectionIdentifierExportPasswordsButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeManageAccount = kItemTypeEnumZero,
  ItemTypeSearchBox,
  ItemTypeHeader,
  ItemTypeSavePasswordsSwitch,
  ItemTypeSavedPassword,  // This is a repeated item type.
  ItemTypeBlacklisted,    // This is a repeated item type.
  ItemTypeExportPasswordsButton,
};

std::vector<std::unique_ptr<autofill::PasswordForm>> CopyOf(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list_copy;
  for (const auto& form : password_list) {
    password_list_copy.push_back(
        std::make_unique<autofill::PasswordForm>(*form));
  }
  return password_list_copy;
}

const CGFloat kSearchCellHeight = 44.0f;

}  // namespace

@interface PasswordFormContentItem : SettingsTextItem
@property(nonatomic) autofill::PasswordForm* form;
@end
@implementation PasswordFormContentItem
@synthesize form = form_;
@end

// Use the type of the items to convey the Saved/Blacklisted status.
@interface SavedFormContentItem : PasswordFormContentItem
@end
@implementation SavedFormContentItem
@end
@interface BlacklistedFormContentItem : PasswordFormContentItem
@end
@implementation BlacklistedFormContentItem
@end

@protocol PasswordExportActivityViewControllerDelegate<NSObject>

// Used to reset the export state when the activity view disappears.
- (void)resetExport;

@end

@interface PasswordExportActivityViewController : UIActivityViewController

- (PasswordExportActivityViewController*)
initWithActivityItems:(NSArray*)activityItems
             delegate:
                 (id<PasswordExportActivityViewControllerDelegate>)delegate;

@end

@implementation PasswordExportActivityViewController {
  __weak id<PasswordExportActivityViewControllerDelegate> _weakDelegate;
}

- (PasswordExportActivityViewController*)
initWithActivityItems:(NSArray*)activityItems
             delegate:
                 (id<PasswordExportActivityViewControllerDelegate>)delegate {
  self = [super initWithActivityItems:activityItems applicationActivities:nil];
  if (self) {
    _weakDelegate = delegate;
  }

  return self;
}

- (void)viewDidDisappear:(BOOL)animated {
  [_weakDelegate resetExport];
  [super viewDidDisappear:animated];
}

@end

@interface SavePasswordsCollectionViewController ()<
    BooleanObserver,
    PasswordDetailsCollectionViewControllerDelegate,
    PasswordExporterDelegate,
    PasswordExportActivityViewControllerDelegate,
    SavePasswordsConsumerDelegate,
    SettingsSearchItemDelegate,
    SuccessfulReauthTimeAccessor> {
  // The observable boolean that binds to the password manager setting state.
  // Saved passwords are only on if the password manager is enabled.
  PrefBackedBoolean* passwordManagerEnabled_;
  // The item related to the switch for the password manager setting.
  LegacySettingsSwitchItem* savePasswordsItem_;
  // The item related to the button for exporting passwords.
  SettingsTextItem* exportPasswordsItem_;
  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> passwordStore_;
  // A helper object for passing data about saved passwords from a finished
  // password store request to the SavePasswordsCollectionViewController.
  std::unique_ptr<ios::SavePasswordsConsumer> savedPasswordsConsumer_;
  // A helper object for passing data about blacklisted sites from a finished
  // password store request to the SavePasswordsCollectionViewController.
  std::unique_ptr<ios::SavePasswordsConsumer> blacklistPasswordsConsumer_;
  // The list of the user's saved passwords.
  std::vector<std::unique_ptr<autofill::PasswordForm>> savedForms_;
  // The list of the user's blacklisted sites.
  std::vector<std::unique_ptr<autofill::PasswordForm>> blacklistedForms_;
  // Map containing duplicates of saved passwords.
  password_manager::DuplicatesMap savedPasswordDuplicates_;
  // Map containing duplicates of blacklisted passwords.
  password_manager::DuplicatesMap blacklistedPasswordDuplicates_;
  // The current Chrome browser state.
  ios::ChromeBrowserState* browserState_;
  // Object storing the time of the previous successful re-authentication.
  // This is meant to be used by the |ReauthenticationModule| for keeping
  // re-authentications valid for a certain time interval within the scope
  // of the Save Passwords Settings.
  NSDate* successfulReauthTime_;
  // Module containing the reauthentication mechanism for viewing and copying
  // passwords.
  ReauthenticationModule* reauthenticationModule_;
  // Boolean containing whether the export operation is ready. This implies that
  // the exporter is idle and there is at least one saved passwords to export.
  BOOL exportReady_;
  // Alert informing the user that passwords are being prepared for
  // export.
  UIAlertController* preparingPasswordsAlert_;
}

// Kick off async request to get logins from password store.
- (void)getLoginsFromPasswordStore;

// Object handling passwords export operations.
@property(nonatomic, strong) PasswordExporter* passwordExporter;

// Current passwords search term.
@property(nonatomic, copy) NSString* searchTerm;

@end

@implementation SavePasswordsCollectionViewController

// Private synthesized properties
@synthesize passwordExporter = passwordExporter_;
@synthesize searchTerm = searchTerm_;

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  MDCCollectionViewFlowLayout* layout =
      [[AlphaAnimatedCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    browserState_ = browserState;
    reauthenticationModule_ = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:self];
    passwordExporter_ = [[PasswordExporter alloc]
        initWithReauthenticationModule:reauthenticationModule_
                              delegate:self];
    self.title = l10n_util::GetNSString(IDS_IOS_PASSWORDS);
    self.collectionViewAccessibilityIdentifier =
        @"SavePasswordsCollectionViewController";
    self.shouldHideDoneButton = YES;
    self.searchTerm = @"";
    passwordStore_ = IOSChromePasswordStoreFactory::GetForBrowserState(
        browserState_, ServiceAccessType::EXPLICIT_ACCESS);
    DCHECK(passwordStore_);
    passwordManagerEnabled_ = [[PrefBackedBoolean alloc]
        initWithPrefService:browserState_->GetPrefs()
                   prefName:password_manager::prefs::kCredentialsEnableService];
    [passwordManagerEnabled_ setObserver:self];
    [self getLoginsFromPasswordStore];
    [self updateEditButton];
    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. Consider moving the other calls on instance methods as well.
    [self loadModel];
    [self updateExportPasswordsButton];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.collectionView.prefetchingEnabled = NO;
}

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  if (!savedForms_.empty() || !blacklistedForms_.empty()) {
    CollectionViewItem* searchItem = [self searchItem];
    [model addSectionWithIdentifier:SectionIdentifierSearchPasswordsBox];
    [model addItem:searchItem
        toSectionWithIdentifier:SectionIdentifierSearchPasswordsBox];
  }

  // Manage account message.
  CollectionViewItem* manageAccountLinkItem = [self manageAccountLinkItem];
  if (manageAccountLinkItem) {
    [model addSectionWithIdentifier:SectionIdentifierMessage];
    [model addItem:manageAccountLinkItem
        toSectionWithIdentifier:SectionIdentifierMessage];
  }

  // Save passwords switch.
  [model addSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
  savePasswordsItem_ = [self savePasswordsItem];
  [model addItem:savePasswordsItem_
      toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];

  // Saved passwords.
  if (!savedForms_.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierSavedPasswords];
    SettingsTextItem* headerItem =
        [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
    headerItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_SAVED_HEADING);
    headerItem.textColor = [[MDCPalette greyPalette] tint500];
    [model setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierSavedPasswords];
  }

  if (!blacklistedForms_.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierBlacklist];
    SettingsTextItem* headerItem =
        [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
    headerItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_EXCEPTIONS_HEADING);
    headerItem.textColor = [[MDCPalette greyPalette] tint500];
    [model setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierBlacklist];
  }

  // Export passwords button.
  [model addSectionWithIdentifier:SectionIdentifierExportPasswordsButton];
  exportPasswordsItem_ = [self exportPasswordsItem];
  [model addItem:exportPasswordsItem_
      toSectionWithIdentifier:SectionIdentifierExportPasswordsButton];

  [self filterItems:self.searchTerm];
}

- (void)reloadData {
  [super reloadData];
  [self updateExportPasswordsButton];
}

- (BOOL)shouldShowEditButton {
  return YES;
}

- (BOOL)editButtonEnabled {
  DCHECK([self shouldShowEditButton]);
  return !savedForms_.empty() || !blacklistedForms_.empty();
}

#pragma mark - Items

- (SettingsSearchItem*)searchItem {
  SettingsSearchItem* item =
      [[SettingsSearchItem alloc] initWithType:ItemTypeSearchBox];
  item.placeholder =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_PLACEHOLDER_SEARCH);
  item.delegate = self;
  return item;
}

- (CollectionViewItem*)manageAccountLinkItem {
  CollectionViewFooterItem* footerItem =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeManageAccount];
  footerItem.cellStyle = CollectionViewCellStyle::kUIKit;
  footerItem.text =
      l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS_MANAGE_ACCOUNT);
  footerItem.linkURL = google_util::AppendGoogleLocaleParam(
      GURL(password_manager::kPasswordManagerAccountDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  footerItem.linkDelegate = self;
  return footerItem;
}

- (LegacySettingsSwitchItem*)savePasswordsItem {
  LegacySettingsSwitchItem* savePasswordsItem =
      [[LegacySettingsSwitchItem alloc]
          initWithType:ItemTypeSavePasswordsSwitch];
  savePasswordsItem.text = l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS);
  savePasswordsItem.on = [passwordManagerEnabled_ value];
  savePasswordsItem.accessibilityIdentifier = @"savePasswordsItem_switch";
  return savePasswordsItem;
}

- (SettingsTextItem*)exportPasswordsItem {
  SettingsTextItem* exportPasswordsItem =
      [[SettingsTextItem alloc] initWithType:ItemTypeExportPasswordsButton];
  exportPasswordsItem.text = l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS);
  exportPasswordsItem.accessibilityIdentifier = @"exportPasswordsItem_button";
  exportPasswordsItem.accessibilityTraits = UIAccessibilityTraitButton;
  return exportPasswordsItem;
}

- (SavedFormContentItem*)savedFormItemWithText:(NSString*)text
                                 andDetailText:(NSString*)detailText
                                       forForm:(autofill::PasswordForm*)form {
  SavedFormContentItem* passwordItem =
      [[SavedFormContentItem alloc] initWithType:ItemTypeSavedPassword];
  passwordItem.text = text;
  passwordItem.form = form;
  passwordItem.detailText = detailText;
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  return passwordItem;
}

- (BlacklistedFormContentItem*)
blacklistedFormItemWithText:(NSString*)text
                    forForm:(autofill::PasswordForm*)form {
  BlacklistedFormContentItem* passwordItem =
      [[BlacklistedFormContentItem alloc] initWithType:ItemTypeBlacklisted];
  passwordItem.text = text;
  passwordItem.form = form;
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  return passwordItem;
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsEditing:(UICollectionView*)collectionView {
  return YES;
}

#pragma mark - MDCCollectionViewStylingDelegate

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierMessage:
    case SectionIdentifierSearchPasswordsBox:
      // Display the message in the default style with no "card" UI and no
      // section padding.
      return MDCCollectionViewCellStyleDefault;
    default:
      return self.styler.cellStyle;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierMessage:
    case SectionIdentifierSearchPasswordsBox:
      // Display the message without any background image or shadowing.
      return YES;
    default:
      return NO;
  }
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeManageAccount:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    case ItemTypeSavedPassword:
      return MDCCellDefaultTwoLineHeight;
    case ItemTypeSearchBox:
      return kSearchCellHeight;
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeSavePasswordsSwitch:
      return YES;
    default:
      return NO;
  }
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  if ([self.collectionViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeSavePasswordsSwitch) {
    LegacySettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(savePasswordsSwitchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, passwordManagerEnabled_);

  // Update the item.
  savePasswordsItem_.on = [passwordManagerEnabled_ value];

  // Update the cell.
  [self reconfigureCellsForItems:@[ savePasswordsItem_ ]];
}

#pragma mark - Actions

- (void)savePasswordsSwitchChanged:(UISwitch*)switchView {
  // Update the setting.
  [passwordManagerEnabled_ setValue:switchView.on];

  // Update the item.
  savePasswordsItem_.on = [passwordManagerEnabled_ value];
}

#pragma mark - SavePasswordsConsumerDelegate

- (void)onGetPasswordStoreResults:
    (std::vector<std::unique_ptr<autofill::PasswordForm>>&)result {
  for (auto it = result.begin(); it != result.end(); ++it) {
    // PasswordForm is needed when user wants to delete the site/password.
    auto form = std::make_unique<autofill::PasswordForm>(**it);
    if (form->blacklisted_by_user)
      blacklistedForms_.push_back(std::move(form));
    else
      savedForms_.push_back(std::move(form));
  }

  password_manager::SortEntriesAndHideDuplicates(&savedForms_,
                                                 &savedPasswordDuplicates_);
  password_manager::SortEntriesAndHideDuplicates(
      &blacklistedForms_, &blacklistedPasswordDuplicates_);

  [self updateEditButton];
  [self reloadData];
}

#pragma mark SettingsSearchItemDelegate

- (void)didRequestSearchForTerm:(NSString*)searchTerm {
  self.searchTerm = searchTerm;
  [self filterItems:searchTerm];

  CollectionViewModel* model = self.collectionViewModel;
  NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];
  if ([model hasSectionForSectionIdentifier:SectionIdentifierSavedPasswords]) {
    NSInteger passwordSection =
        [model sectionForSectionIdentifier:SectionIdentifierSavedPasswords];
    [indexSet addIndex:passwordSection];
  }
  if ([model hasSectionForSectionIdentifier:SectionIdentifierBlacklist]) {
    NSInteger blacklistedSection =
        [model sectionForSectionIdentifier:SectionIdentifierBlacklist];
    [indexSet addIndex:blacklistedSection];
  }
  if (indexSet.count > 0) {
    [UIView setAnimationsEnabled:NO];
    [self.collectionView reloadSections:indexSet];
    [UIView setAnimationsEnabled:YES];
  }
}

#pragma mark - Private methods

// Builds the filtered list of passwords/blacklisted based on given
// |searchTerm|.
- (void)filterItems:(NSString*)searchTerm {
  CollectionViewModel* model = self.collectionViewModel;

  if (!savedForms_.empty()) {
    [model deleteAllItemsFromSectionWithIdentifier:
               SectionIdentifierSavedPasswords];
    for (const auto& form : savedForms_) {
      NSString* text = base::SysUTF8ToNSString(
          password_manager::GetShownOriginAndLinkUrl(*form).first);
      NSString* detailText = base::SysUTF16ToNSString(form->username_value);
      bool hidden =
          searchTerm.length > 0 &&
          ![text localizedCaseInsensitiveContainsString:searchTerm] &&
          ![detailText localizedCaseInsensitiveContainsString:searchTerm];
      if (hidden)
        continue;
      [model addItem:[self savedFormItemWithText:text
                                   andDetailText:detailText
                                         forForm:form.get()]
          toSectionWithIdentifier:SectionIdentifierSavedPasswords];
    }
  }

  if (!blacklistedForms_.empty()) {
    [model deleteAllItemsFromSectionWithIdentifier:SectionIdentifierBlacklist];
    for (const auto& form : blacklistedForms_) {
      NSString* text = base::SysUTF8ToNSString(
          password_manager::GetShownOriginAndLinkUrl(*form).first);
      bool hidden = searchTerm.length > 0 &&
                    ![text localizedCaseInsensitiveContainsString:searchTerm];
      if (hidden)
        continue;
      [model addItem:[self blacklistedFormItemWithText:text forForm:form.get()]
          toSectionWithIdentifier:SectionIdentifierBlacklist];
    }
  }
}

// Starts requests for saved and blacklisted passwords to the store.
- (void)getLoginsFromPasswordStore {
  savedPasswordsConsumer_.reset(new ios::SavePasswordsConsumer(self));
  passwordStore_->GetAutofillableLogins(savedPasswordsConsumer_.get());
  blacklistPasswordsConsumer_.reset(new ios::SavePasswordsConsumer(self));
  passwordStore_->GetBlacklistLogins(blacklistPasswordsConsumer_.get());
}

- (void)updateExportPasswordsButton {
  if (!exportPasswordsItem_)
    return;
  if (!savedForms_.empty() &&
      self.passwordExporter.exportState == ExportState::IDLE) {
    exportReady_ = YES;
    if (![self.editor isEditing]) {
      [self setExportPasswordsButtonEnabled:YES];
    }
  } else {
    exportReady_ = NO;
    [self setExportPasswordsButtonEnabled:NO];
  }
}

- (void)setExportPasswordsButtonEnabled:(BOOL)enabled {
  if (enabled) {
    DCHECK(exportReady_ && ![self.editor isEditing]);
    exportPasswordsItem_.textColor = [[MDCPalette greyPalette] tint900];
    exportPasswordsItem_.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    exportPasswordsItem_.textColor = [[MDCPalette greyPalette] tint500];
    exportPasswordsItem_.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  [self reconfigureCellsForItems:@[ exportPasswordsItem_ ]];
}

- (void)startPasswordsExportFlow {
  UIAlertController* exportConfirmation = [UIAlertController
      alertControllerWithTitle:nil
                       message:l10n_util::GetNSString(
                                   IDS_IOS_EXPORT_PASSWORDS_ALERT_MESSAGE)
                preferredStyle:UIAlertControllerStyleActionSheet];
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_EXPORT_PASSWORDS_CANCEL_BUTTON)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               UMA_HISTOGRAM_ENUMERATION(
                                   "PasswordManager.ExportPasswordsToCSVResult",
                                   password_manager::metrics_util::
                                       ExportPasswordsResult::USER_ABORTED,
                                   password_manager::metrics_util::
                                       ExportPasswordsResult::COUNT);
                             }];
  [exportConfirmation addAction:cancelAction];

  __weak SavePasswordsCollectionViewController* weakSelf = self;
  UIAlertAction* exportAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                SavePasswordsCollectionViewController* strongSelf = weakSelf;
                if (!strongSelf) {
                  return;
                }
                [strongSelf.passwordExporter
                    startExportFlow:CopyOf(strongSelf->savedForms_)];
              }];

  [exportConfirmation addAction:exportAction];

  [self presentViewController:exportConfirmation animated:YES completion:nil];
}

// Removes the given section if it exists and if isEmpty is true.
- (void)clearSectionWithIdentifier:(NSInteger)sectionIdentifier
                           ifEmpty:(bool)isEmpty {
  CollectionViewModel* model = self.collectionViewModel;
  if (isEmpty && [model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    [model removeSectionWithIdentifier:sectionIdentifier];
    [[self collectionView]
        deleteSections:[NSIndexSet indexSetWithIndex:section]];
  }
}

#pragma mark UICollectionViewDelegate

- (void)openDetailedViewForForm:(const autofill::PasswordForm&)form {
  PasswordDetailsCollectionViewController* controller =
      [[PasswordDetailsCollectionViewController alloc]
            initWithPasswordForm:form
                        delegate:self
          reauthenticationModule:reauthenticationModule_];
  controller.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  // Actions should only take effect when not in editing mode.
  if ([self.editor isEditing]) {
    return;
  }

  CollectionViewModel* model = self.collectionViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeManageAccount:
    case ItemTypeHeader:
    case ItemTypeSavePasswordsSwitch:
    case ItemTypeSearchBox:
      break;
    case ItemTypeSavedPassword: {
      DCHECK_EQ(SectionIdentifierSavedPasswords,
                [model sectionIdentifierForSection:indexPath.section]);
      SavedFormContentItem* saveFormItem =
          base::mac::ObjCCastStrict<SavedFormContentItem>(
              [model itemAtIndexPath:indexPath]);
      [self openDetailedViewForForm:*saveFormItem.form];
      break;
    }
    case ItemTypeBlacklisted: {
      DCHECK_EQ(SectionIdentifierBlacklist,
                [model sectionIdentifierForSection:indexPath.section]);
      BlacklistedFormContentItem* blacklistedItem =
          base::mac::ObjCCastStrict<BlacklistedFormContentItem>(
              [model itemAtIndexPath:indexPath]);
      [self openDetailedViewForForm:*blacklistedItem.form];
      break;
    }
    case ItemTypeExportPasswordsButton:
      DCHECK_EQ(SectionIdentifierExportPasswordsButton,
                [model sectionIdentifierForSection:indexPath.section]);
      if (exportReady_) {
        [self startPasswordsExportFlow];
      }
      break;
    default:
      NOTREACHED();
  }
}

#pragma mark MDCCollectionViewEditingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    canEditItemAtIndexPath:(NSIndexPath*)indexPath {
  // Only password cells are editable.
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  return [item isKindOfClass:[SavedFormContentItem class]] ||
         [item isKindOfClass:[BlacklistedFormContentItem class]];
}

- (void)collectionViewWillBeginEditing:(UICollectionView*)collectionView {
  [super collectionViewWillBeginEditing:collectionView];

  [self setSavePasswordsSwitchItemEnabled:NO];
  [self setExportPasswordsButtonEnabled:NO];
  [self setSearchPasswordsItemEnabled:NO];
}

- (void)collectionViewWillEndEditing:(UICollectionView*)collectionView {
  [super collectionViewWillEndEditing:collectionView];

  [self setSavePasswordsSwitchItemEnabled:YES];
  if (exportReady_) {
    [self setExportPasswordsButtonEnabled:YES];
  }
  [self setSearchPasswordsItemEnabled:YES];
}

- (void)collectionView:(UICollectionView*)collectionView
    willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  // Ensure indexPaths are sorted to maintain delete logic, and keep track of
  // number of items deleted to adjust index for accessing elements in the
  // forms vectors.
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  auto passwordIterator = savedForms_.begin();
  auto passwordEndIterator = savedForms_.end();
  auto blacklistedIterator = blacklistedForms_.begin();
  auto blacklistedEndIterator = blacklistedForms_.end();
  for (NSIndexPath* indexPath in sortedIndexPaths) {
    // Only form items are editable.
    PasswordFormContentItem* item =
        base::mac::ObjCCastStrict<PasswordFormContentItem>(
            [self.collectionViewModel itemAtIndexPath:indexPath]);
    BOOL blacklisted = [item isKindOfClass:[BlacklistedFormContentItem class]];
    auto& forms = blacklisted ? blacklistedForms_ : savedForms_;
    auto& duplicates =
        blacklisted ? blacklistedPasswordDuplicates_ : savedPasswordDuplicates_;

    const autofill::PasswordForm& deletedForm = *item.form;
    auto begin = blacklisted ? blacklistedIterator : passwordIterator;
    auto end = blacklisted ? blacklistedEndIterator : passwordEndIterator;

    auto formIterator = std::find_if(
        begin, end,
        [&deletedForm](const std::unique_ptr<autofill::PasswordForm>& value) {
          return *value == deletedForm;
        });
    DCHECK(formIterator != end);

    std::unique_ptr<autofill::PasswordForm> form = std::move(*formIterator);
    std::string key = password_manager::CreateSortKey(*form);
    auto duplicatesRange = duplicates.equal_range(key);
    for (auto iterator = duplicatesRange.first;
         iterator != duplicatesRange.second; ++iterator) {
      passwordStore_->RemoveLogin(*(iterator->second));
    }
    duplicates.erase(key);

    formIterator = forms.erase(formIterator);
    passwordStore_->RemoveLogin(*form);

    // Keep track of where we are in the current list.
    if (blacklisted) {
      blacklistedIterator = formIterator;
    } else {
      passwordIterator = formIterator;
    }
  }

  // Must call super at the end of the child implementation.
  [super collectionView:collectionView willDeleteItemsAtIndexPaths:indexPaths];
}

- (void)collectionView:(UICollectionView*)collectionView
    didDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  // Remove empty sections.
  __weak SavePasswordsCollectionViewController* weakSelf = self;
  [self.collectionView performBatchUpdates:^{
    SavePasswordsCollectionViewController* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    // Delete in reverse order of section indexes (bottom up of section
    // displayed), so that indexes in model matches those in the view.  if we
    // don't we'll cause a crash.
    [strongSelf
        clearSectionWithIdentifier:SectionIdentifierBlacklist
                           ifEmpty:strongSelf->blacklistedForms_.empty()];
    [strongSelf clearSectionWithIdentifier:SectionIdentifierSavedPasswords
                                   ifEmpty:strongSelf->savedForms_.empty()];
    [strongSelf
        clearSectionWithIdentifier:SectionIdentifierSearchPasswordsBox
                           ifEmpty:strongSelf->savedForms_.empty() &&
                                   strongSelf->blacklistedForms_.empty()];
  }
      completion:^(BOOL finished) {
        SavePasswordsCollectionViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        if (![strongSelf editButtonEnabled]) {
          [strongSelf.editor setEditing:NO];
        }
        [strongSelf updateEditButton];
        [strongSelf updateExportPasswordsButton];
      }];
}

#pragma mark PasswordDetailsCollectionViewControllerDelegate

- (void)deletePassword:(const autofill::PasswordForm&)form {
  passwordStore_->RemoveLogin(form);

  std::vector<std::unique_ptr<autofill::PasswordForm>>& forms =
      form.blacklisted_by_user ? blacklistedForms_ : savedForms_;
  auto iterator = std::find_if(
      forms.begin(), forms.end(),
      [&form](const std::unique_ptr<autofill::PasswordForm>& value) {
        return *value == form;
      });
  DCHECK(iterator != forms.end());
  forms.erase(iterator);

  password_manager::DuplicatesMap& duplicates =
      form.blacklisted_by_user ? blacklistedPasswordDuplicates_
                               : savedPasswordDuplicates_;
  std::string key = password_manager::CreateSortKey(form);
  auto duplicatesRange = duplicates.equal_range(key);
  for (auto iterator = duplicatesRange.first;
       iterator != duplicatesRange.second; ++iterator) {
    passwordStore_->RemoveLogin(*(iterator->second));
  }
  duplicates.erase(key);

  [self updateEditButton];
  [self reloadData];
  [self.navigationController popViewControllerAnimated:YES];
}

#pragma mark SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  successfulReauthTime_ = [[NSDate alloc] init];
}

- (NSDate*)lastSuccessfulReauthTime {
  return successfulReauthTime_;
}

#pragma mark PasswordExporterDelegate

- (void)showSetPasscodeDialog {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE)
                       message:
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_EXPORT_PASSWORDS_SET_UP_SCREENLOCK_CONTENT)
                preferredStyle:UIAlertControllerStyleAlert];

  ProceduralBlockWithURL blockOpenURL = BlockToOpenURL(self, self.dispatcher);
  UIAlertAction* learnAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                blockOpenURL(GURL(kPasscodeArticleURL));
              }];
  [alertController addAction:learnAction];
  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alertController addAction:okAction];
  alertController.preferredAction = okAction;
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)showPreparingPasswordsAlert {
  preparingPasswordsAlert_ = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS_PREPARING_ALERT_TITLE)
                       message:nil
                preferredStyle:UIAlertControllerStyleAlert];
  __weak SavePasswordsCollectionViewController* weakSelf = self;
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_EXPORT_PASSWORDS_CANCEL_BUTTON)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction*) {
                               [weakSelf.passwordExporter cancelExport];
                             }];
  [preparingPasswordsAlert_ addAction:cancelAction];
  [self presentViewController:preparingPasswordsAlert_
                     animated:YES
                   completion:nil];
}

- (void)showExportErrorAlertWithLocalizedReason:(NSString*)localizedReason {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_EXPORT_PASSWORDS_FAILED_ALERT_TITLE)
                       message:localizedReason
                preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alertController addAction:okAction];
  [self presentViewController:alertController];
}

- (void)showActivityViewWithActivityItems:(NSArray*)activityItems
                        completionHandler:(void (^)(NSString* activityType,
                                                    BOOL completed,
                                                    NSArray* returnedItems,
                                                    NSError* activityError))
                                              completionHandler {
  PasswordExportActivityViewController* activityViewController =
      [[PasswordExportActivityViewController alloc]
          initWithActivityItems:activityItems
                       delegate:self];
  NSArray* excludedActivityTypes = @[
    UIActivityTypeAddToReadingList, UIActivityTypeAirDrop,
    UIActivityTypeCopyToPasteboard, UIActivityTypeOpenInIBooks,
    UIActivityTypePostToFacebook, UIActivityTypePostToFlickr,
    UIActivityTypePostToTencentWeibo, UIActivityTypePostToTwitter,
    UIActivityTypePostToVimeo, UIActivityTypePostToWeibo, UIActivityTypePrint
  ];
  [activityViewController setExcludedActivityTypes:excludedActivityTypes];

  [activityViewController setCompletionWithItemsHandler:completionHandler];

  UIView* sourceView = nil;
  CGRect sourceRect = CGRectZero;
  if (IsIPadIdiom() && !IsCompactWidth()) {
    NSIndexPath* indexPath = [self.collectionViewModel
        indexPathForItemType:ItemTypeExportPasswordsButton
           sectionIdentifier:SectionIdentifierExportPasswordsButton];
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];
    sourceView = self.collectionView;
    sourceRect = cell.frame;
  }
  activityViewController.modalPresentationStyle = UIModalPresentationPopover;
  activityViewController.popoverPresentationController.sourceView = sourceView;
  activityViewController.popoverPresentationController.sourceRect = sourceRect;
  activityViewController.popoverPresentationController
      .permittedArrowDirections =
      UIPopoverArrowDirectionDown | UIPopoverArrowDirectionDown;

  [self presentViewController:activityViewController];
}

#pragma mark - PasswordExportActivityViewControllerDelegate

- (void)resetExport {
  [self.passwordExporter resetExportState];
}

#pragma mark Helper methods

- (void)presentViewController:(UIViewController*)viewController {
  if (self.presentedViewController == preparingPasswordsAlert_ &&
      !preparingPasswordsAlert_.beingDismissed) {
    __weak SavePasswordsCollectionViewController* weakSelf = self;
    [self dismissViewControllerAnimated:YES
                             completion:^{
                               [weakSelf presentViewController:viewController
                                                      animated:YES
                                                    completion:nil];
                             }];
  } else {
    [self presentViewController:viewController animated:YES completion:nil];
  }
}

// Sets the save passwords switch item's enabled status to |enabled| and
// reconfigures the corresponding cell.
- (void)setSavePasswordsSwitchItemEnabled:(BOOL)enabled {
  CollectionViewModel* model = self.collectionViewModel;

  if (![model hasItemForItemType:ItemTypeSavePasswordsSwitch
               sectionIdentifier:SectionIdentifierSavePasswordsSwitch]) {
    return;
  }
  NSIndexPath* switchPath =
      [model indexPathForItemType:ItemTypeSavePasswordsSwitch
                sectionIdentifier:SectionIdentifierSavePasswordsSwitch];
  LegacySettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<LegacySettingsSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  [switchItem setEnabled:enabled];
  [self reconfigureCellsForItems:@[ switchItem ]];
}

// Sets the search passwords item's enabled status to |enabled| and
// reconfigures the corresponding cell.
- (void)setSearchPasswordsItemEnabled:(BOOL)enabled {
  CollectionViewModel* model = self.collectionViewModel;

  if (![model hasItemForItemType:ItemTypeSearchBox
               sectionIdentifier:SectionIdentifierSearchPasswordsBox]) {
    return;
  }
  NSIndexPath* itemPath =
      [model indexPathForItemType:ItemTypeSearchBox
                sectionIdentifier:SectionIdentifierSearchPasswordsBox];
  SettingsSearchItem* searchItem =
      base::mac::ObjCCastStrict<SettingsSearchItem>(
          [model itemAtIndexPath:itemPath]);
  [searchItem setEnabled:enabled];

  NSIndexPath* indexPath =
      [self.collectionViewModel indexPathForItem:searchItem];
  MDCCollectionViewCell* cell =
      base::mac::ObjCCastStrict<MDCCollectionViewCell>(
          [self.collectionView cellForItemAtIndexPath:indexPath]);

  // |cell| may be nil if the row is not currently on screen. If that's the case
  // and we are disabling we force the keyboard down (since the cell can't do it
  // for us).
  if (cell) {
    [searchItem configureCell:cell];
  } else if (!enabled) {
    [self.view endEditing:YES];
  }
}

#pragma mark - Testing

- (void)setReauthenticationModuleForExporter:
    (id<ReauthenticationProtocol>)reauthenticationModule {
  passwordExporter_ = [[PasswordExporter alloc]
      initWithReauthenticationModule:reauthenticationModule
                            delegate:self];
}

- (PasswordExporter*)getPasswordExporter {
  return passwordExporter_;
}

@end
