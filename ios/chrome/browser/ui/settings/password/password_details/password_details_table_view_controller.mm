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
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_menu_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
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
  SectionIdentifierCompromisedInfo
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeWebsite = kItemTypeEnumZero,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeFederation,
  ItemTypeChangePasswordButton,
  ItemTypeChangePasswordRecommendation,
  ItemTypeFooter
};

typedef NS_ENUM(NSInteger, ReauthenticationReason) {
  ReauthenticationReasonShow = 0,
  ReauthenticationReasonCopy,
  ReauthenticationReasonEdit,
  ReauthenticationReasonReplacePassword,
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
// image icon in the |usernameTextItem| cell.
@property(nonatomic, weak) UIView* usernameErrorAnchorView;

// Denotes the type of the credential passed to this coordinator. Could be
// blocked, federated, new or regular.
@property(nonatomic, assign) CredentialType credentialType;

@end

@implementation PasswordDetailsTableViewController

#pragma mark - ViewController Life Cycle.

- (instancetype)initWithCredentialType:(CredentialType)credentialType {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _credentialType = credentialType;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPasswordDetailsViewControllerId;
  self.tableView.allowsSelectionDuringEditing = YES;

  if (self.credentialType == CredentialTypeNew) {
    // TODO(crbug.com/1226006): Use i18n strings for the buttons.
    self.navigationItem.title = @"Add Password";

    // Adds 'Cancel' and 'Save' buttons to Navigation bar.
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(didTapCancelButton:)];
    self.navigationItem.leftBarButtonItem.accessibilityIdentifier =
        kPasswordsAddPasswordCancelButtonId;

    self.navigationItem.rightBarButtonItem =
        [[UIBarButtonItem alloc] initWithTitle:@"Save"
                                         style:UIBarButtonItemStyleDone
                                        target:self
                                        action:@selector(didTapSaveButton:)];
    self.navigationItem.rightBarButtonItem.enabled = NO;
    self.navigationItem.rightBarButtonItem.accessibilityIdentifier =
        kPasswordsAddPasswordSaveButtonId;

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
  if (self.credentialType != CredentialTypeNew) {
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
  bool isAddingPasswordsEnabled = base::FeatureList::IsEnabled(
      password_manager::features::kSupportForAddPasswordsInSettings);

  self.websiteTextItem = [self websiteItem];
  if (isAddingPasswordsEnabled) {
    [model addSectionWithIdentifier:SectionIdentifierSite];

    [model addItem:self.websiteTextItem
        toSectionWithIdentifier:SectionIdentifierSite];
  }

  [model addSectionWithIdentifier:SectionIdentifierPassword];
  if (!isAddingPasswordsEnabled) {
    [model addItem:self.websiteTextItem
        toSectionWithIdentifier:SectionIdentifierPassword];
  }
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

        if (self.password.changePasswordURL.is_valid()) {
          [model addItem:[self changePasswordItem]
              toSectionWithIdentifier:SectionIdentifierCompromisedInfo];
        }

        [model addItem:[self changePasswordRecommendationItem]
            toSectionWithIdentifier:SectionIdentifierCompromisedInfo];
      }
    }
  }

  if (self.credentialType == CredentialTypeNew) {
    [model setFooter:[self footerItem]
        forSectionWithIdentifier:SectionIdentifierPassword];
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
  item.hideIcon = YES;
  item.keyboardType = UIKeyboardTypeURL;
  if (base::FeatureList::IsEnabled(
          password_manager::features::kSupportForAddPasswordsInSettings)) {
    // TODO(crbug.com/1226006): Use i18n string for the placeholder.
    item.textFieldPlaceholder = @"example.com";
  }
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
  if (self.credentialType != CredentialTypeFederation &&
      base::FeatureList::IsEnabled(
          password_manager::features::kEditPasswordsInSettings)) {
    item.textFieldEnabled = self.tableView.editing;
    item.hideIcon = !self.tableView.editing;
    item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
    item.returnKeyType = UIReturnKeyDone;
    item.delegate = self;
  } else {
    item.textFieldEnabled = NO;
    item.hideIcon = YES;
  }
  item.textFieldEnabled |= (self.credentialType == CredentialTypeNew);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kSupportForAddPasswordsInSettings)) {
    // TODO(crbug.com/1226006): Use i18n string for the placeholder.
    item.textFieldPlaceholder = @"optional";
  }
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
  item.hideIcon =
      (self.credentialType == CredentialTypeNew) || !self.tableView.editing;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.keyboardType = UIKeyboardTypeURL;
  item.returnKeyType = UIReturnKeyDone;
  item.delegate = self;
  if (base::FeatureList::IsEnabled(
          password_manager::features::kSupportForAddPasswordsInSettings)) {
    // TODO(crbug.com/1226006): Use i18n string for the placeholder.
    item.textFieldPlaceholder = @"password";
  }

  // During editing password is exposed so eye icon shouldn't be shown.
  if (!self.tableView.editing) {
    NSString* image = [self isPasswordShown] ? @"infobar_hide_password_icon"
                                             : @"infobar_reveal_password_icon";
    item.identifyingIcon = [[UIImage imageNamed:image]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    item.identifyingIconEnabled = YES;
    item.identifyingIconAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
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
  item.detailText =
      l10n_util::GetNSString(IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION);
  item.image = [self compromisedIcon];
  return item;
}

- (TableViewLinkHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  // TODO(crbug.com/1226006): Use i18n string.
  item.text = @"Make sure you're saving your current password for this site";
  return item;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.credentialType == CredentialTypeNew) {
    return;
  }
  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeWebsite:
    case ItemTypeFederation:
      [self ensureContextMenuShownForItemType:itemType
                                    tableView:tableView
                                  atIndexPath:indexPath];
      break;
    case ItemTypeChangePasswordRecommendation:
    case ItemTypeFooter:
      break;
    case ItemTypeUsername: {
      if (base::FeatureList::IsEnabled(
              password_manager::features::kEditPasswordsInSettings) &&
          self.tableView.editing) {
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
        DCHECK(self.commandsHandler);
        DCHECK(self.password.changePasswordURL.is_valid());
        OpenNewTabCommand* command = [OpenNewTabCommand
            commandWithURLFromChrome:self.password.changePasswordURL];
        UmaHistogramEnumeration("PasswordManager.BulkCheck.UserAction",
                                PasswordCheckInteraction::kChangePassword);
        [self.commandsHandler closeSettingsUIAndOpenURL:command];
      }
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
  return !self.editing && self.credentialType != CredentialTypeNew;
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
    case ItemTypeWebsite:
    case ItemTypeFederation:
    case ItemTypeChangePasswordButton:
    case ItemTypeFooter:
      break;
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
      return NO;
    case ItemTypeUsername:
      return base::FeatureList::IsEnabled(
          password_manager::features::kEditPasswordsInSettings);
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
  self.navigationItem.rightBarButtonItem.enabled = !duplicateFound;
  // TODO(crbug.com/1226006): Update model when a duplicate is found.
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  [self reconfigureCellsForItems:@[
    self.websiteTextItem, self.usernameTextItem, self.passwordTextItem
  ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  // TODO(crbug.com/1226006): Add validations for the site.
  if (self.credentialType == CredentialTypeNew) {
    [self.delegate
        checkForDuplicatesWithSite:self.websiteTextItem.textFieldValue
                          username:self.usernameTextItem.textFieldValue];
  } else {
    BOOL isInputValid = [self checkIfValidSite] & [self checkIfValidUsername] &
                        [self checkIfValidPassword];
    self.navigationItem.rightBarButtonItem.enabled = isInputValid;
  }
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  // Check if the item is equal to the current username or password item as when
  // editing finished reloadData is called.
  if (tableViewItem == self.websiteTextItem) {
    [self reconfigureCellsForItems:@[ self.websiteTextItem ]];
  } else if (tableViewItem == self.usernameTextItem) {
    [self reconfigureCellsForItems:@[ self.usernameTextItem ]];
  } else if (tableViewItem == self.passwordTextItem) {
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
  [self.delegate
      passwordDetailsViewController:self
      didAddPasswordDetailsWithSite:self.websiteTextItem.textFieldValue
                           username:self.usernameTextItem.textFieldValue
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

// Shows reauthentication dialog if needed. If the reauthentication is
// successful reveals the password.
- (void)attemptToShowPasswordFor:(ReauthenticationReason)reason {
  // If password was already shown (before editing or copying) we don't need to
  // request reauth again.
  if (self.isPasswordShown) {
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
      // Called super because we want to update only |tableView.editing|.
      [super editButtonPressed];
      [self reloadData];
      break;
    case ReauthenticationReasonReplacePassword:
      NOTREACHED();
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
    case ReauthenticationReasonReplacePassword:
      // TODO(crbug.com/1226006): Use i18n string.
      return @"Replace Password";
  }
}

// Shows a snack bar with |message| and provides haptic feedback. The haptic
// feedback is either for success or for error, depending on |success|.
- (void)showToast:(NSString*)message forSuccess:(BOOL)success {
  TriggerHapticFeedbackForNotification(success
                                           ? UINotificationFeedbackTypeSuccess
                                           : UINotificationFeedbackTypeError);
  [self.commandsHandler showSnackbarWithMessage:message
                                     buttonText:nil
                                  messageAction:nil
                               completionAction:nil];
}

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:cellPath];
  switch (static_cast<ItemType>(itemType)) {
    case ItemTypeUsername:
      return base::FeatureList::IsEnabled(
          password_manager::features::kEditPasswordsInSettings);
    case ItemTypePassword:
      return YES;
    case ItemTypeWebsite:
    case ItemTypeFederation:
    case ItemTypeChangePasswordButton:
    case ItemTypeChangePasswordRecommendation:
    case ItemTypeFooter:
      return NO;
  }
}

- (BOOL)checkIfValidSite {
  BOOL siteEmpty = [self.websiteTextItem.textFieldValue length] == 0;
  self.websiteTextItem.hasValidText = !siteEmpty;

  [self reconfigureCellsForItems:@[ self.websiteTextItem ]];
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

  self.usernameTextItem.hasValidText = !showUsernameAlreadyUsed;
  self.usernameTextItem.identifyingIconEnabled = showUsernameAlreadyUsed;

  [self reconfigureCellsForItems:@[ self.usernameTextItem ]];
  return !showUsernameAlreadyUsed;
}

// Checks if the password is valid and updates item accordingly.
- (BOOL)checkIfValidPassword {
  DCHECK(self.password.password || (self.credentialType == CredentialTypeNew));

  BOOL passwordEmpty = [self.passwordTextItem.textFieldValue length] == 0;
  self.passwordTextItem.hasValidText = !passwordEmpty;

  [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
  return !passwordEmpty;
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
    [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
  } else {
    if (self.credentialType == CredentialTypeNew) {
      self.passwordTextItem.textFieldSecureTextEntry = NO;
      self.passwordShown = YES;
      self.passwordTextItem.identifyingIcon =
          [[UIImage imageNamed:@"infobar_hide_password_icon"]
              imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
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

// Logs metrics for the given reauthentication |result| (success, failure or
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
    case ReauthenticationReasonReplacePassword:
      NOTREACHED();
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

- (void)validateUserAndReplaceExistingCredential {
  if ([self.reauthModule canAttemptReauth]) {
    __weak __typeof(self) weakSelf = self;
    void (^editPasswordConfirmationHandler)(ReauthenticationResult) =
        ^(ReauthenticationResult result) {
          PasswordDetailsTableViewController* strongSelf = weakSelf;
          if (!strongSelf)
            return;
          [strongSelf logPasswordSettingsReauthResult:result];

          if (result == ReauthenticationResult::kFailure) {
            return;
          }

          [strongSelf.delegate didConfirmReplaceExistingCredential];
        };

    NSString* reauthReason =
        [self localizedStringForReason:ReauthenticationReasonReplacePassword];

    [self.reauthModule
        attemptReauthWithLocalizedReason:reauthReason
                    canReusePreviousAuth:YES
                                 handler:editPasswordConfirmationHandler];
  } else {
    DCHECK(self.addPasswordHandler);
    [self.addPasswordHandler showPasscodeDialog];
  }
}

@end
