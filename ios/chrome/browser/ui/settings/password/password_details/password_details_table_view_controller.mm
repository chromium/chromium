// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_menu_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::UmaHistogramEnumeration;
using password_manager::metrics_util::LogPasswordSettingsReauthResult;
using password_manager::metrics_util::PasswordCheckInteraction;
using password_manager::metrics_util::ReauthResult;

// Padding used between the image and the text labels.
const CGFloat kWarningIconSize = 20;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPassword = kSectionIdentifierEnumZero,
  SectionIdentifierSite,
  SectionIdentifierCompromisedInfo,
  SectionIdentifierDuplicate,
  SectionIdentifierFooter,
  SectionIdentifierTLDFooter
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeWebsite = kItemTypeEnumZero,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeFederation,
  ItemTypeChangePasswordButton,
  ItemTypeChangePasswordRecommendation,
  ItemTypeFooter,
  ItemTypeDuplicateCredentialButton,
  ItemTypeDuplicateCredentialMessage
};

typedef NS_ENUM(NSInteger, ReauthenticationReason) {
  ReauthenticationReasonShow = 0,
  ReauthenticationReasonCopy,
  ReauthenticationReasonEdit,
};

}  // namespace

@interface PasswordDetailsTableViewController () <TableViewTextEditItemDelegate>

// Password which is shown on the screen.
@property(nonatomic, strong) PasswordDetails* password;

// Whether the password is shown in plain text form or in masked form.
@property(nonatomic, assign, getter=isPasswordShown) BOOL passwordShown;

// The text item related to the site value.
@property(nonatomic, strong) TableViewTextEditItem* websiteTextItem;

// The text item related to the username value.
@property(nonatomic, strong) TableViewTextEditItem* usernameTextItem;

// The text item related to the password value.
@property(nonatomic, strong) TableViewTextEditItem* passwordTextItem;

// The view used to anchor error alert which is shown for the username. This is
// image icon in the `usernameTextItem` cell.
@property(nonatomic, weak) UIView* usernameErrorAnchorView;

// Denotes the type of the credential passed to this coordinator. Could be
// blocked, federated, new or regular.
@property(nonatomic, assign) CredentialType credentialType;

// If YES, denotes that the credential with the same website/username
// combination already exists. Used when creating a new credential.
@property(nonatomic, assign) BOOL isDuplicatedCredential;

// Denotes that the save button in the add credential view can be enabled after
// basic validation of data on all the fields. Does not account for whether the
// duplicate credential exists or not.
@property(nonatomic, assign) BOOL shouldEnableSave;

// Yes, when the message for top-level domain missing is shown.
@property(nonatomic, assign) BOOL isTLDMissingMessageShown;

// If YES, the password details are shown without requiring any authentication.
@property(nonatomic, assign) BOOL showPasswordWithoutAuth;

// Stores the user email if the user is authenticated amd syncing passwords.
@property(nonatomic, readonly) NSString* syncingUserEmail;

@end

@implementation PasswordDetailsTableViewController

#pragma mark - ViewController Life Cycle.

- (instancetype)initWithCredentialType:(CredentialType)credentialType
                      syncingUserEmail:(NSString*)syncingUserEmail {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _credentialType = credentialType;
    _isDuplicatedCredential = NO;
    _shouldEnableSave = NO;
    _showPasswordWithoutAuth = NO;
    _isTLDMissingMessageShown = NO;
    _syncingUserEmail = syncingUserEmail;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPasswordDetailsViewControllerId;
  self.tableView.allowsSelectionDuringEditing = YES;

  if (self.credentialType == CredentialTypeNew) {
    self.navigationItem.title = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_SETTINGS_ADD_PASSWORD_MANUALLY_TITLE);

    // Adds 'Cancel' and 'Save' buttons to Navigation bar.
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(didTapCancelButton:)];
    self.navigationItem.leftBarButtonItem.accessibilityIdentifier =
        kPasswordsAddPasswordCancelButtonId;

    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_PASSWORD_SETTINGS_SAVE_BUTTON)
                style:UIBarButtonItemStyleDone
               target:self
               action:@selector(didTapSaveButton:)];
    self.navigationItem.rightBarButtonItem.enabled = NO;
    self.navigationItem.rightBarButtonItem.accessibilityIdentifier =
        kPasswordsAddPasswordSaveButtonId;

    password_manager::metrics_util::
        LogUserInteractionsWhenAddingCredentialFromSettings(
            password_manager::metrics_util::
                AddCredentialFromSettingsUserInteractions::kAddDialogOpened);

    [self loadModel];
  } else {
    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.lineBreakMode = NSLineBreakByTruncatingHead;
    titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    titleLabel.adjustsFontForContentSizeCategory = YES;
    titleLabel.text = self.password.origin;
    self.navigationItem.titleView = titleLabel;
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  if (self.credentialType == CredentialTypeNew) {
    password_manager::metrics_util::
        LogUserInteractionsWhenAddingCredentialFromSettings(
            password_manager::metrics_util::
                AddCredentialFromSettingsUserInteractions::kAddDialogClosed);
  } else {
    [self.handler passwordDetailsTableViewControllerDidDisappear];
  }
  [super viewDidDisappear:animated];
}

#pragma mark - ChromeTableViewController

- (void)editButtonPressed {
  // If password value is missing, proceed with editing without
  // reauthentication.
  if (![self.password.password length]) {
    [super editButtonPressed];
    return;
  }

  // Request reauthentication before revealing password during editing.
  // Editing mode will be entered on successful reauth.
  if (!self.tableView.editing && !self.isPasswordShown) {
    [self attemptToShowPasswordFor:ReauthenticationReasonEdit];
    return;
  }

  if (self.tableView.editing) {
    // If site, password or username value was changed show confirmation dialog
    // before saving password. Editing mode will be exited only if user confirm
    // saving.
    if (![self.password.website
            isEqualToString:self.websiteTextItem.textFieldValue] ||
        ![self.password.password
            isEqualToString:self.passwordTextItem.textFieldValue] ||
        ![self.password.username
            isEqualToString:self.usernameTextItem.textFieldValue]) {
      DCHECK(self.handler);
      [self.handler showPasswordEditDialogWithOrigin:self.password.origin];
    } else {
      [self passwordEditingConfirmed];
    }
    return;
  }

  [super editButtonPressed];
  [self reloadData];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  self.websiteTextItem = [self websiteItem];

  [model addSectionWithIdentifier:SectionIdentifierSite];

  [model addItem:self.websiteTextItem
      toSectionWithIdentifier:SectionIdentifierSite];

  [model addSectionWithIdentifier:SectionIdentifierTLDFooter];

  [model addSectionWithIdentifier:SectionIdentifierPassword];
  // Blocked passwords don't have username and password value.
  if (self.credentialType != CredentialTypeBlocked) {
    self.usernameTextItem = [self usernameItem];
    [model addItem:self.usernameTextItem
        toSectionWithIdentifier:SectionIdentifierPassword];

    if (self.credentialType == CredentialTypeFederation) {
      // Federated password forms don't have password value.
      [model addItem:[self federationItem]
          toSectionWithIdentifier:SectionIdentifierPassword];
    } else {
      self.passwordTextItem = [self passwordItem];
      [model addItem:self.passwordTextItem
          toSectionWithIdentifier:SectionIdentifierPassword];

      if (self.password.isCompromised) {
        [model addSectionWithIdentifier:SectionIdentifierCompromisedInfo];
        if (base::FeatureList::IsEnabled(
                password_manager::features::
                    kIOSEnablePasswordManagerBrandingUpdate)) {
          [model addItem:[self changePasswordRecommendationItem]
              toSectionWithIdentifier:SectionIdentifierCompromisedInfo];

          if (self.password.changePasswordURL.is_valid()) {
            [model addItem:[self changePasswordItem]
                toSectionWithIdentifier:SectionIdentifierCompromisedInfo];
          }
        } else {
          if (self.password.changePasswordURL.is_valid()) {
            [model addItem:[self changePasswordItem]
                toSectionWithIdentifier:SectionIdentifierCompromisedInfo];
          }

          [model addItem:[self changePasswordRecommendationItem]
              toSectionWithIdentifier:SectionIdentifierCompromisedInfo];
        }
      }
    }
  }

  if (self.credentialType == CredentialTypeNew) {
    [model addSectionWithIdentifier:SectionIdentifierFooter];
    [model setFooter:[self footerItem]
        forSectionWithIdentifier:SectionIdentifierFooter];
  }
}

- (BOOL)showCancelDuringEditing {
  return YES;
}

#pragma mark - Items

- (TableViewTextEditItem*)websiteItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeWebsite];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.textFieldName = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  item.textFieldValue = self.password.website;  // Empty for a new form.
  // TODO(crbug.com/1226006): The website field should be editable in the edit
  // mode.
  item.textFieldEnabled = (self.credentialType == CredentialTypeNew);
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.hideIcon = (self.credentialType != CredentialTypeNew);
  item.keyboardType = UIKeyboardTypeURL;
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_WEBSITE_PLACEHOLDER_TEXT);
  if (self.credentialType == CredentialTypeNew) {
    item.delegate = self;
  }
  return item;
}

- (TableViewTextEditItem*)usernameItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeUsername];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  item.textFieldValue = self.password.username;  // Empty for a new form.
  // If password is missing (federated credential) don't allow to edit username.
  if (self.credentialType != CredentialTypeFederation) {
    item.textFieldEnabled = self.tableView.editing;
    item.hideIcon = !self.tableView.editing;
    item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
    item.delegate = self;
  } else {
    item.textFieldEnabled = NO;
    item.hideIcon = YES;
  }
  item.textFieldEnabled |= (self.credentialType == CredentialTypeNew);
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_USERNAME_PLACEHOLDER_TEXT);
  item.hideIcon = NO;
  return item;
}

- (TableViewTextEditItem*)passwordItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypePassword];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  if (self.credentialType == CredentialTypeNew) {
    item.textFieldSecureTextEntry = ![self isPasswordShown];
  } else {
    item.textFieldValue = [self isPasswordShown] || self.tableView.editing
                              ? self.password.password
                              : kMaskedPassword;
  }
  item.textFieldEnabled =
      (self.credentialType == CredentialTypeNew) || self.tableView.editing;
  if (self.credentialType == CredentialTypeNew) {
    item.hideIcon = NO;
  } else {
    item.hideIcon = !self.tableView.editing;
  }
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.keyboardType = UIKeyboardTypeURL;
  item.returnKeyType = UIReturnKeyDone;
  item.delegate = self;
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_PASSWORD_PLACEHOLDER_TEXT);

  // During editing password is exposed so eye icon shouldn't be shown.
  if (!self.tableView.editing) {
    NSString* image = [self isPasswordShown] ? @"infobar_hide_password_icon"
                                             : @"infobar_reveal_password_icon";
    item.identifyingIcon = [[UIImage imageNamed:image]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    item.identifyingIconEnabled = YES;
    item.identifyingIconAccessibilityLabel = l10n_util::GetNSString(
        [self isPasswordShown] ? IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON
                               : IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
  }
  return item;
}

- (TableViewTextEditItem*)federationItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeFederation];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION);
  item.textFieldValue = self.password.federation;
  item.textFieldEnabled = NO;
  item.hideIcon = YES;
  return item;
}

- (TableViewTextItem*)changePasswordItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeChangePasswordButton];
  item.text = l10n_util::GetNSString(IDS_IOS_CHANGE_COMPROMISED_PASSWORD);
  item.textColor = self.tableView.editing
                       ? [UIColor colorNamed:kTextSecondaryColor]
                       : [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  return item;
}

- (SettingsImageDetailTextItem*)changePasswordRecommendationItem {
  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:ItemTypeChangePasswordRecommendation];
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSEnablePasswordManagerBrandingUpdate)) {
    item.detailText = l10n_util::GetNSString(
        IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED);
  } else {
    item.detailText =
        l10n_util::GetNSString(IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION);
  }
  item.image = [self compromisedIcon];
  return item;
}

- (TableViewTextItem*)duplicatePasswordViewButtonItem {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:ItemTypeDuplicateCredentialButton];
  item.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_VIEW_PASSWORD_BUTTON);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  return item;
}

- (SettingsImageDetailTextItem*)duplicatePasswordMessageItem {
  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:ItemTypeDuplicateCredentialMessage];
  if (self.usernameTextItem &&
      [self.usernameTextItem.textFieldValue length] > 0) {
    item.detailText = l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_PASSWORDS_DUPLICATE_SECTION_ALERT_DESCRIPTION,
        base::SysNSStringToUTF16(self.usernameTextItem.textFieldValue),
        base::SysNSStringToUTF16(self.websiteTextItem.textFieldValue));
  } else {
    item.detailText = l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_PASSWORDS_DUPLICATE_SECTION_ALERT_DESCRIPTION_WITHOUT_USERNAME,
        base::SysNSStringToUTF16(self.websiteTextItem.textFieldValue));
  }
  item.image = [[UIImage imageNamed:@"table_view_cell_error_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  item.imageViewTintColor = [UIColor colorNamed:kRedColor];
  return item;
}

- (TableViewLinkHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  item.text =
      [NSString stringWithFormat:@"%@\n\n%@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_ADD_PASSWORD_DESCRIPTION),
                                 [self footerText]];
  return item;
}

- (TableViewLinkHeaderFooterItem*)TLDMessageFooterItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  item.text = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_PASSWORDS_MISSING_TLD_DESCRIPTION,
      base::SysNSStringToUTF16([self.websiteTextItem.textFieldValue
          stringByAppendingString:@".com"]));
  return item;
}

- (NSString*)footerText {
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSEnablePasswordManagerBrandingUpdate)) {
    if (self.syncingUserEmail) {
      return l10n_util::GetNSStringF(
          IDS_IOS_SETTINGS_ADD_PASSWORD_FOOTER_BRANDED,
          base::SysNSStringToUTF16(self.syncingUserEmail));
    }

    return l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORD_FOOTER_NOT_SYNCING);
  } else {
    if (self.syncingUserEmail) {
      return l10n_util::GetNSStringF(
          IDS_IOS_SETTINGS_ADD_PASSWORD_FOOTER,
          base::SysNSStringToUTF16(self.syncingUserEmail));
    }

    return l10n_util::GetNSString(
        IDS_IOS_SETTINGS_ADD_PASSWORD_FOOTER_NON_SYNCING);
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  if (self.credentialType == CredentialTypeNew &&
      itemType != ItemTypeDuplicateCredentialButton) {
    return;
  }
  switch (itemType) {
    case ItemTypeWebsite:
    case ItemTypeFederation:
      [self ensureContextMenuShownForItemType:itemType
                                    tableView:tableView
                                  atIndexPath:indexPath];
      break;
    case ItemTypeChangePasswordRecommendation:
    case ItemTypeFooter:
    case ItemTypeDuplicateCredentialMessage:
      break;
    case ItemTypeUsername: {
      if (self.tableView.editing) {
        UITableViewCell* cell =
            [self.tableView cellForRowAtIndexPath:indexPath];
        TableViewTextEditCell* textFieldCell =
            base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
        [textFieldCell.textField becomeFirstResponder];
      } else {
        [self ensureContextMenuShownForItemType:itemType
                                      tableView:tableView
                                    atIndexPath:indexPath];
      }
      break;
    }
    case ItemTypePassword: {
      if (self.tableView.editing) {
        UITableViewCell* cell =
            [self.tableView cellForRowAtIndexPath:indexPath];
        TableViewTextEditCell* textFieldCell =
            base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
        [textFieldCell.textField becomeFirstResponder];
      } else {
        [self ensureContextMenuShownForItemType:itemType
                                      tableView:tableView
                                    atIndexPath:indexPath];
      }
      break;
    }
    case ItemTypeChangePasswordButton:
      if (!self.tableView.editing) {
        DCHECK(self.applicationCommandsHandler);
        DCHECK(self.password.changePasswordURL.is_valid());
        OpenNewTabCommand* command = [OpenNewTabCommand
            commandWithURLFromChrome:self.password.changePasswordURL];
        UmaHistogramEnumeration("PasswordManager.BulkCheck.UserAction",
                                PasswordCheckInteraction::kChangePassword);
        [self.applicationCommandsHandler closeSettingsUIAndOpenURL:command];
      }
      break;
    case ItemTypeDuplicateCredentialButton:
      password_manager::metrics_util::
          LogUserInteractionsWhenAddingCredentialFromSettings(
              password_manager::metrics_util::
                  AddCredentialFromSettingsUserInteractions::
                      kDuplicateCredentialViewed);
      [self reauthAndShowExistingCredential];
      break;
  }
}

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewCellEditingStyleNone;
}

- (BOOL)tableView:(UITableView*)tableview
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

// If the context menu is not shown for a given item type, constructs that
// menu and shows it. This method should only be called for item types
// representing the cells with the site, username and password.
- (void)ensureContextMenuShownForItemType:(NSInteger)itemType
                                tableView:(UITableView*)tableView
                              atIndexPath:(NSIndexPath*)indexPath {
  UIMenuController* menu = [UIMenuController sharedMenuController];
  if (![menu isMenuVisible]) {
    menu.menuItems = [self menuItemsForItemType:itemType];

    [menu showMenuFromView:tableView
                      rect:[tableView rectForRowAtIndexPath:indexPath]];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  BOOL isNewCredentialDuplicateButton =
      (self.isDuplicatedCredential &&
       itemType == ItemTypeDuplicateCredentialButton);
  return (!self.editing && self.credentialType != CredentialTypeNew) ||
         isNewCredentialDuplicateButton;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (sectionIdentifier == SectionIdentifierFooter ||
      sectionIdentifier == SectionIdentifierTLDFooter) {
    return 0;
  }
  return [super tableView:tableView heightForHeaderInSection:section];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  if (sectionIdentifier == SectionIdentifierSite) {
    return 0;
  }
  if ((sectionIdentifier == SectionIdentifierPassword &&
       !self.isDuplicatedCredential) ||
      sectionIdentifier == SectionIdentifierDuplicate) {
    return 0;
  }
  return [super tableView:tableView heightForFooterInSection:section];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  cell.tag = itemType;
  cell.selectionStyle = UITableViewCellSelectionStyleDefault;

  switch (itemType) {
    case ItemTypeUsername: {
      TableViewTextEditCell* textFieldCell =
          base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
      textFieldCell.textField.delegate = self;
      [textFieldCell.identifyingIconButton
                 addTarget:self
                    action:@selector(didTapUsernameErrorInfo:)
          forControlEvents:UIControlEventTouchUpInside];
      self.usernameErrorAnchorView = textFieldCell.iconView;
      break;
    }
    case ItemTypePassword: {
      TableViewTextEditCell* textFieldCell =
          base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
      textFieldCell.textField.delegate = self;
      [textFieldCell.identifyingIconButton
                 addTarget:self
                    action:@selector(didTapShowHideButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeWebsite: {
      if (self.credentialType == CredentialTypeNew) {
        TableViewTextEditCell* textFieldCell =
            base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
        textFieldCell.textField.delegate = self;
      }
      break;
    }
    case ItemTypeFederation:
    case ItemTypeChangePasswordButton:
    case ItemTypeDuplicateCredentialMessage:
    case ItemTypeFooter:
      break;
    case ItemTypeDuplicateCredentialButton:
    case ItemTypeChangePasswordRecommendation:
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
  }
  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeWebsite:
    case ItemTypeFederation:
    case ItemTypeFooter:
    case ItemTypeDuplicateCredentialMessage:
    case ItemTypeDuplicateCredentialButton:
      return NO;
    case ItemTypeUsername:
    case ItemTypePassword:
      return YES;
  }
  return NO;
}

#pragma mark - PasswordDetailsConsumer

- (void)setPassword:(PasswordDetails*)password {
  _password = password;
  [self reloadData];
}

#pragma mark - AddPasswordDetailsConsumer

- (void)onDuplicateCheckCompletion:(BOOL)duplicateFound {
  if (duplicateFound == self.isDuplicatedCredential) {
    return;
  }

  self.isDuplicatedCredential = duplicateFound;
  [self toggleNavigationBarRightButtonItem];
  TableViewModel* model = self.tableViewModel;
  if (duplicateFound) {
    password_manager::metrics_util::
        LogUserInteractionsWhenAddingCredentialFromSettings(
            password_manager::metrics_util::
                AddCredentialFromSettingsUserInteractions::
                    kDuplicatedCredentialEntered);
    [self
        performBatchTableViewUpdates:^{
          NSUInteger passwordSectionIndex = [self.tableViewModel
              sectionForSectionIdentifier:SectionIdentifierPassword];
          [model insertSectionWithIdentifier:SectionIdentifierDuplicate
                                     atIndex:passwordSectionIndex + 1];
          [self.tableView
                insertSections:[NSIndexSet
                                   indexSetWithIndex:passwordSectionIndex + 1]
              withRowAnimation:UITableViewRowAnimationTop];
          [model addItem:[self duplicatePasswordMessageItem]
              toSectionWithIdentifier:SectionIdentifierDuplicate];
          [model addItem:[self duplicatePasswordViewButtonItem]
              toSectionWithIdentifier:SectionIdentifierDuplicate];
          if (self.usernameTextItem &&
              [self.usernameTextItem.textFieldValue length] > 0) {
            self.usernameTextItem.hasValidText = NO;
            [self reconfigureCellsForItems:@[ self.usernameTextItem ]];
          } else {
            self.websiteTextItem.hasValidText = NO;
            [self reconfigureCellsForItems:@[ self.websiteTextItem ]];
          }
        }
                          completion:nil];
  } else {
    [self
        performBatchTableViewUpdates:^{
          [self removeSectionWithIdentifier:SectionIdentifierDuplicate
                           withRowAnimation:UITableViewRowAnimationTop];
          self.usernameTextItem.hasValidText = YES;
          self.websiteTextItem.hasValidText = YES;
          [self reconfigureCellsForItems:@[
            self.websiteTextItem, self.usernameTextItem
          ]];
        }
                          completion:nil];
  }
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  [self reconfigureCellsForItems:@[
    self.websiteTextItem, self.usernameTextItem, self.passwordTextItem
  ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  if (tableViewItem == self.websiteTextItem &&
      self.credentialType == CredentialTypeNew) {
    [self.delegate setWebsiteURL:self.websiteTextItem.textFieldValue];
    if (self.isTLDMissingMessageShown) {
      self.isTLDMissingMessageShown = NO;
      [self
          performBatchTableViewUpdates:^{
            [self.tableViewModel setFooter:nil
                  forSectionWithIdentifier:SectionIdentifierTLDFooter];
            NSUInteger index = [self.tableViewModel
                sectionForSectionIdentifier:SectionIdentifierTLDFooter];
            [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                          withRowAnimation:UITableViewRowAnimationNone];
          }
                            completion:nil];
    }
  }

  BOOL siteValid = [self checkIfValidSite];
  BOOL usernameValid = [self checkIfValidUsername];
  BOOL passwordValid = [self checkIfValidPassword];

  self.shouldEnableSave = (siteValid && usernameValid && passwordValid);
  [self toggleNavigationBarRightButtonItem];

  if (self.credentialType == CredentialTypeNew) {
    [self.delegate checkForDuplicates:self.usernameTextItem.textFieldValue];
  }
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  if (tableViewItem == self.websiteTextItem) {
    if (!self.isDuplicatedCredential) {
      self.websiteTextItem.hasValidText = [self checkIfValidSite];
    }
    if ([self.websiteTextItem.textFieldValue length] > 0 &&
        [self.delegate isTLDMissing]) {
      [self showTLDMissingSection];
      self.websiteTextItem.hasValidText = NO;
    }
    [self reconfigureCellsForItems:@[ self.websiteTextItem ]];
  } else if (tableViewItem == self.usernameTextItem) {
    [self reconfigureCellsForItems:@[ self.usernameTextItem ]];
  } else if (tableViewItem == self.passwordTextItem) {
    self.passwordTextItem.hasValidText = [self checkIfValidPassword];
    [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
  }
}

#pragma mark - Actions

// Dimisses this view controller when Cancel button is tapped.
- (void)didTapCancelButton:(id)sender {
  [self.delegate didCancelAddPasswordDetails];
}

// Handles Save button tap on adding new credentials.
- (void)didTapSaveButton:(id)sender {
  if ([self.websiteTextItem.textFieldValue length] > 0 &&
      [self.delegate isTLDMissing]) {
    [self showTLDMissingSection];
    return;
  }
  [self.delegate
      passwordDetailsViewController:self
              didAddPasswordDetails:self.usernameTextItem.textFieldValue
                           password:self.passwordTextItem.textFieldValue];
}

#pragma mark - SettingsRootTableViewController

// Called when user tapped Delete button during editing. It means presented
// password should be deleted.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  DCHECK(self.handler);
  // Pass origin only if password is present as confirmation message makes
  // sense only in this case.
  if ([self.password.password length]) {
    [self.handler
        showPasswordDeleteDialogWithOrigin:self.password.origin
                       compromisedPassword:self.password.isCompromised];
  } else {
    [self.handler showPasswordDeleteDialogWithOrigin:nil
                                 compromisedPassword:NO];
  }
}

- (BOOL)shouldHideToolbar {
  return !self.editing;
}

#pragma mark - Private

// Applies tint colour and resizes image.
- (UIImage*)compromisedIcon {
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSEnablePasswordManagerBrandingUpdate)) {
    return [UIImage imageNamed:@"round_settings_unsafe_state"];
  } else {
    UIImage* image = [UIImage imageNamed:@"settings_unsafe_state"];
    UIImage* newImage =
        [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    UIGraphicsBeginImageContextWithOptions(
        CGSizeMake(kWarningIconSize, kWarningIconSize), NO, 0.0);
    [[UIColor colorNamed:kTextSecondaryColor] set];
    [newImage drawInRect:CGRectMake(0, 0, kWarningIconSize, kWarningIconSize)];
    newImage = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return newImage;
  }
}

// Shows reauthentication dialog if needed. If the reauthentication is
// successful reveals the password.
- (void)attemptToShowPasswordFor:(ReauthenticationReason)reason {
  // If password was already shown (before editing or copying) or the flag to
  // override auth is YES, we don't need to request reauth again.
  if (self.isPasswordShown || self.showPasswordWithoutAuth) {
    [self showPasswordFor:reason];
    return;
  }

  if ([self.reauthModule canAttemptReauth]) {
    __weak __typeof(self) weakSelf = self;
    void (^showPasswordHandler)(ReauthenticationResult) =
        ^(ReauthenticationResult result) {
          PasswordDetailsTableViewController* strongSelf = weakSelf;
          if (!strongSelf)
            return;
          [strongSelf logPasswordSettingsReauthResult:result];

          if (result == ReauthenticationResult::kFailure) {
            if (reason == ReauthenticationReasonCopy) {
              [strongSelf
                   showToast:
                       l10n_util::GetNSString(
                           IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE)
                  forSuccess:NO];
            }
            return;
          }

          [strongSelf showPasswordFor:reason];
        };

    [self.reauthModule
        attemptReauthWithLocalizedReason:[self localizedStringForReason:reason]
                    canReusePreviousAuth:YES
                                 handler:showPasswordHandler];
  } else {
    DCHECK(self.handler);
    [self.handler showPasscodeDialog];
  }
}

// Reveals password to the user.
- (void)showPasswordFor:(ReauthenticationReason)reason {
  switch (reason) {
    case ReauthenticationReasonShow:
      self.passwordShown = YES;
      self.passwordTextItem.textFieldValue = self.password.password;
      self.passwordTextItem.identifyingIcon =
          [[UIImage imageNamed:@"infobar_hide_password_icon"]
              imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      self.passwordTextItem.identifyingIconAccessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON);
      [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
      if (self.password.compromised) {
        UmaHistogramEnumeration("PasswordManager.BulkCheck.UserAction",
                                PasswordCheckInteraction::kShowPassword);
      }
      break;
    case ReauthenticationReasonCopy: {
      UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
      generalPasteboard.string = self.password.password;
      [self showToast:l10n_util::GetNSString(
                          IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE)
           forSuccess:YES];
      break;
    }
    case ReauthenticationReasonEdit:
      // Called super because we want to update only `tableView.editing`.
      [super editButtonPressed];
      [self reloadData];
      break;
  }
  [self logPasswordAccessWith:reason];
}

// Returns localized reason for reauthentication dialog.
- (NSString*)localizedStringForReason:(ReauthenticationReason)reason {
  switch (reason) {
    case ReauthenticationReasonShow:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW);
    case ReauthenticationReasonCopy:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_COPY);
    case ReauthenticationReasonEdit:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_EDIT);
  }
}

// Shows a snack bar with `message` and provides haptic feedback. The haptic
// feedback is either for success or for error, depending on `success`. Deselect
// cell if there was one selected.
- (void)showToast:(NSString*)message forSuccess:(BOOL)success {
  TriggerHapticFeedbackForNotification(success
                                           ? UINotificationFeedbackTypeSuccess
                                           : UINotificationFeedbackTypeError);
  [self.snackbarCommandsHandler showSnackbarWithMessage:message
                                             buttonText:nil
                                          messageAction:nil
                                       completionAction:nil];

  if ([self.tableView indexPathForSelectedRow]) {
    [self.tableView
        deselectRowAtIndexPath:[self.tableView indexPathForSelectedRow]
                      animated:YES];
  }
}

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:cellPath];
  switch (static_cast<ItemType>(itemType)) {
    case ItemTypeUsername:
    case ItemTypePassword:
      return YES;
    case ItemTypeWebsite:
      if (self.credentialType == CredentialTypeNew) {
        return YES;
      }
      return NO;
    case ItemTypeFederation:
    case ItemTypeChangePasswordButton:
    case ItemTypeChangePasswordRecommendation:
    case ItemTypeDuplicateCredentialMessage:
    case ItemTypeDuplicateCredentialButton:
    case ItemTypeFooter:
      return NO;
  }
}

- (BOOL)checkIfValidSite {
  BOOL siteEmpty = [self.websiteTextItem.textFieldValue length] == 0;
  if (self.credentialType == CredentialTypeNew) {
    if (!siteEmpty && !self.isTLDMissingMessageShown &&
        !self.isDuplicatedCredential) {
      self.websiteTextItem.hasValidText = YES;
      [self reconfigureCellsForItems:@[ self.websiteTextItem ]];
    }
  } else {
    self.websiteTextItem.hasValidText = !siteEmpty;
    [self reconfigureCellsForItems:@[ self.websiteTextItem ]];
  }
  return !siteEmpty;
}

// Checks if the username is valid and updates item accordingly.
- (BOOL)checkIfValidUsername {
  DCHECK(self.password.username || (self.credentialType == CredentialTypeNew));
  NSString* newUsernameValue = self.usernameTextItem.textFieldValue;
  BOOL usernameChanged =
      ![newUsernameValue isEqualToString:self.password.username];
  BOOL showUsernameAlreadyUsed =
      usernameChanged && [self.delegate isUsernameReused:newUsernameValue];

  if (self.credentialType == CredentialTypeNew) {
    if (!self.isDuplicatedCredential) {
      self.usernameTextItem.hasValidText = YES;
      [self reconfigureCellsForItems:@[ self.usernameTextItem ]];
    }
  } else {
    self.usernameTextItem.hasValidText = !showUsernameAlreadyUsed;
    [self reconfigureCellsForItems:@[ self.usernameTextItem ]];
  }
  self.usernameTextItem.hasValidText = !showUsernameAlreadyUsed;
  self.usernameTextItem.identifyingIconEnabled = showUsernameAlreadyUsed;

  return !showUsernameAlreadyUsed;
}

// Checks if the password is valid and updates item accordingly.
- (BOOL)checkIfValidPassword {
  DCHECK(self.password.password || (self.credentialType == CredentialTypeNew));

  BOOL passwordEmpty = [self.passwordTextItem.textFieldValue length] == 0;
  if (self.credentialType == CredentialTypeNew) {
    if (!passwordEmpty) {
      self.passwordTextItem.hasValidText = YES;
      [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
    }
  } else {
    self.passwordTextItem.hasValidText = !passwordEmpty;
    [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
  }

  return !passwordEmpty;
}

// Removes the given section if it exists.
- (void)removeSectionWithIdentifier:(NSInteger)sectionIdentifier
                   withRowAnimation:(UITableViewRowAnimation)animation {
  TableViewModel* model = self.tableViewModel;
  if ([model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    [model removeSectionWithIdentifier:sectionIdentifier];
    [[self tableView] deleteSections:[NSIndexSet indexSetWithIndex:section]
                    withRowAnimation:animation];
  }
}

- (void)reauthAndShowExistingCredential {
  if ([self.reauthModule canAttemptReauth]) {
    __weak __typeof(self) weakSelf = self;
    void (^viewExistingPasswordHandler)(ReauthenticationResult) =
        ^(ReauthenticationResult result) {
          PasswordDetailsTableViewController* strongSelf = weakSelf;
          if (!strongSelf)
            return;
          [strongSelf logPasswordSettingsReauthResult:result];

          if (result == ReauthenticationResult::kFailure) {
            return;
          }

          [strongSelf.delegate
              showExistingCredential:strongSelf.usernameTextItem
                                         .textFieldValue];
        };

    [self.reauthModule
        attemptReauthWithLocalizedReason:
            [self localizedStringForReason:ReauthenticationReasonShow]
                    canReusePreviousAuth:YES
                                 handler:viewExistingPasswordHandler];
  } else {
    DCHECK(self.addPasswordHandler);
    [self.addPasswordHandler showPasscodeDialog];
  }
}

// Enables/Disables the right bar button item in the navigation bar.
- (void)toggleNavigationBarRightButtonItem {
  self.navigationItem.rightBarButtonItem.enabled =
      !self.isDuplicatedCredential && self.shouldEnableSave &&
      [self.delegate isURLValid] && !self.isTLDMissingMessageShown;
}

// Shows the section with the error message for top-level domain missing.
- (void)showTLDMissingSection {
  if (self.isTLDMissingMessageShown) {
    return;
  }

  self.navigationItem.rightBarButtonItem.enabled = NO;
  self.isTLDMissingMessageShown = YES;
  [self
      performBatchTableViewUpdates:^{
        [self.tableViewModel setFooter:[self TLDMessageFooterItem]
              forSectionWithIdentifier:SectionIdentifierTLDFooter];
        NSUInteger index = [self.tableViewModel
            sectionForSectionIdentifier:SectionIdentifierTLDFooter];
        [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                      withRowAnimation:UITableViewRowAnimationNone];
      }
                        completion:nil];
}

#pragma mark - Actions

// Called when the user tapped on the show/hide button near password.
- (void)didTapShowHideButton:(UIButton*)buttonView {
  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:NO];
  if (self.isPasswordShown) {
    self.passwordShown = NO;
    if (self.credentialType == CredentialTypeNew) {
      self.passwordTextItem.textFieldSecureTextEntry = YES;
    } else {
      self.passwordTextItem.textFieldValue = kMaskedPassword;
    }
    self.passwordTextItem.identifyingIcon =
        [[UIImage imageNamed:@"infobar_reveal_password_icon"]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    self.passwordTextItem.identifyingIconAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
    [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
  } else {
    if (self.credentialType == CredentialTypeNew) {
      self.passwordTextItem.textFieldSecureTextEntry = NO;
      self.passwordShown = YES;
      self.passwordTextItem.identifyingIcon =
          [[UIImage imageNamed:@"infobar_hide_password_icon"]
              imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      self.passwordTextItem.identifyingIconAccessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON);
      [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
    } else {
      [self attemptToShowPasswordFor:ReauthenticationReasonShow];
    }
  }
}

// Called when the user tap error info icon in the username input.
- (void)didTapUsernameErrorInfo:(UIButton*)buttonView {
  NSString* text = l10n_util::GetNSString(IDS_IOS_USERNAME_ALREADY_USED);

  NSAttributedString* attributedText = [[NSAttributedString alloc]
      initWithString:text
          attributes:@{
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextSecondaryColor],
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
          }];

  PopoverLabelViewController* errorInfoPopover =
      [[PopoverLabelViewController alloc]
          initWithPrimaryAttributedString:attributedText
                secondaryAttributedString:nil];

  errorInfoPopover.popoverPresentationController.sourceView =
      self.usernameErrorAnchorView;
  errorInfoPopover.popoverPresentationController.sourceRect =
      self.usernameErrorAnchorView.bounds;
  errorInfoPopover.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
  [self presentViewController:errorInfoPopover animated:YES completion:nil];
}

// Returns an array of UIMenuItems to display in a context menu on the site
// cell.
- (NSArray*)menuItemsForItemType:(NSInteger)itemType {
  PasswordDetailsMenuItem* copyOption = [[PasswordDetailsMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_COPY_MENU_ITEM)
             action:@selector(copyPasswordDetails:)];
  copyOption.itemType = itemType;
  return @[ copyOption ];
}

// Copies the password information to system pasteboard and shows a toast of
// success/failure.
- (void)copyPasswordDetails:(id)sender {
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  UIMenuController* menu = base::mac::ObjCCastStrict<UIMenuController>(sender);
  PasswordDetailsMenuItem* menuItem =
      base::mac::ObjCCastStrict<PasswordDetailsMenuItem>(
          menu.menuItems.firstObject);

  NSString* message = nil;

  switch (menuItem.itemType) {
    case ItemTypeWebsite:
      generalPasteboard.string = self.password.website;
      message =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE);
      break;
    case ItemTypeUsername:
      generalPasteboard.string = self.password.username;
      message =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE);
      break;
    case ItemTypeFederation:
      generalPasteboard.string = self.password.federation;
      return;
    case ItemTypePassword:
      [self attemptToShowPasswordFor:ReauthenticationReasonCopy];
      return;
    case ItemTypeDuplicateCredentialMessage:
    case ItemTypeDuplicateCredentialButton:
    case ItemTypeFooter:
      NOTREACHED();
      return;
  }
  [self showToast:message forSuccess:YES];
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(copyPasswordDetails:)) {
    return YES;
  }
  return NO;
}

#pragma mark - Metrics

// Logs metrics for the given reauthentication `result` (success, failure or
// skipped).
- (void)logPasswordSettingsReauthResult:(ReauthenticationResult)result {
  switch (result) {
    case ReauthenticationResult::kSuccess:
      LogPasswordSettingsReauthResult(ReauthResult::kSuccess);
      break;
    case ReauthenticationResult::kFailure:
      LogPasswordSettingsReauthResult(ReauthResult::kFailure);
      break;
    case ReauthenticationResult::kSkipped:
      LogPasswordSettingsReauthResult(ReauthResult::kSkipped);
      break;
  }
}

- (void)logPasswordAccessWith:(ReauthenticationReason)reason {
  switch (reason) {
    case ReauthenticationReasonShow:
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AccessPasswordInSettings",
          password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
          password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
      break;
    case ReauthenticationReasonCopy:
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AccessPasswordInSettings",
          password_manager::metrics_util::ACCESS_PASSWORD_COPIED,
          password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
      break;
    case ReauthenticationReasonEdit:
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AccessPasswordInSettings",
          password_manager::metrics_util::ACCESS_PASSWORD_EDITED,
          password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
      break;
  }
}

#pragma mark - Public

- (void)passwordEditingConfirmed {
  self.password.username = self.usernameTextItem.textFieldValue;
  self.password.password = self.passwordTextItem.textFieldValue;
  [self.delegate passwordDetailsViewController:self
                        didEditPasswordDetails:self.password];
  [super editButtonPressed];
  if (self.password.compromised) {
    UmaHistogramEnumeration("PasswordManager.BulkCheck.UserAction",
                            PasswordCheckInteraction::kEditPassword);
  }
  [self reloadData];
}

- (void)showEditViewWithoutAuthentication {
  self.showPasswordWithoutAuth = YES;
  [self editButtonPressed];
}

@end
