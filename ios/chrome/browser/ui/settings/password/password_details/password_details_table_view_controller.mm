// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_menu_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

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
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeWebsite = kItemTypeEnumZero,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeFederation,
  ItemTypeChangePasswordButton,
  ItemTypeChangePasswordRecommendation,
};

typedef NS_ENUM(NSInteger, ReauthenticationReason) {
  ReauthenticationReasonShow = 0,
  ReauthenticationReasonCopy,
  ReauthenticationReasonEdit,
};

// Return if the feature flag for the password grouping is enabled.
// TODO(crbug.com/1359392): Remove this when kPasswordsGrouping flag is removed.
bool IsPasswordGroupingEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordsGrouping);
}

// Size of the symbols.
const CGFloat kSymbolSize = 15;
const CGFloat kCompromisedPasswordSymbolSize = 22;

}  // namespace

// Contains the website, username and password text edit items.
@interface PasswordDetailsInfoItem : NSObject

// The text item related to the site value.
@property(nonatomic, strong) TableViewTextEditItem* websiteTextItem;

// The text item related to the username value.
@property(nonatomic, strong) TableViewTextEditItem* usernameTextItem;

// The text item related to the password value.
@property(nonatomic, strong) TableViewTextEditItem* passwordTextItem;

@end
@implementation PasswordDetailsInfoItem
@end

@interface PasswordDetailsTableViewController () <
    TableViewTextEditItemDelegate> {
  // Index of the password the user wants to reveal.
  NSInteger _passwordIndexToReveal;

  // Title label displayed in the navigation bar.
  UILabel* _titleLabel;
}

// Array of passwords that are shown on the screen.
@property(nonatomic, strong) NSArray<PasswordDetails*>* passwords;

@property(nonatomic, strong) NSString* pageTitle;

// Whether the password is shown in plain text form or in masked form.
@property(nonatomic, assign, getter=isPasswordShown) BOOL passwordShown;

// Array of the password details info items used by the table view model.
@property(nonatomic, strong)
    NSMutableArray<PasswordDetailsInfoItem*>* passwordDetailsInfoItems;

// The view used to anchor error alert which is shown for the username. This is
// image icon in the `usernameTextItem` cell.
@property(nonatomic, weak) UIView* usernameErrorAnchorView;

// Denotes that the Done button in editing mode can be enabled after
// basic validation of data on all the fields. Does not account for whether the
// duplicate credential exists or not.
@property(nonatomic, assign) BOOL shouldEnableEditDoneButton;

// If YES, the password details are shown without requiring any authentication.
@property(nonatomic, assign) BOOL showPasswordWithoutAuth;

// Stores the user email if the user is authenticated amd syncing passwords.
@property(nonatomic, readonly) NSString* syncingUserEmail;

@end

@implementation PasswordDetailsTableViewController

#pragma mark - ViewController Life Cycle.

- (instancetype)initWithSyncingUserEmail:(NSString*)syncingUserEmail {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _shouldEnableEditDoneButton = NO;
    _showPasswordWithoutAuth = NO;
    _syncingUserEmail = syncingUserEmail;
    _passwordIndexToReveal = 0;

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.lineBreakMode = NSLineBreakByTruncatingHead;
    _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    self.navigationItem.titleView = _titleLabel;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier = kPasswordDetailsViewControllerId;
  self.tableView.allowsSelectionDuringEditing = YES;
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.handler passwordDetailsTableViewControllerDidDisappear];
  [super viewDidDisappear:animated];
}

#pragma mark - ChromeTableViewController

- (void)editButtonPressed {
  // If there are no passwords, proceed with editing without
  // reauthentication.
  if (![self hasAtLeastOnePassword]) {
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
    if ([self fieldsDidChange]) {
      DCHECK(self.handler);
      // TODO(crbug.com/1401035): Show Password Edit Dialog when Password
      // Grouping is enabled.
      [self.handler showPasswordEditDialogWithOrigin:self.pageTitle];
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

  self.passwordDetailsInfoItems = [[NSMutableArray alloc] init];

  for (PasswordDetails* passwordDetails in _passwords) {
    [self addPasswordDetailsToModel:passwordDetails];
  }
}

- (BOOL)showCancelDuringEditing {
  return YES;
}

#pragma mark - Items

- (TableViewTextEditItem*)websiteItemForPasswordDetails:
    (PasswordDetails*)passwordDetails {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeWebsite];
  item.textFieldBackgroundColor = [UIColor clearColor];
  // TODO(crbug.com/1358982): Update text to "Sites".
  item.textFieldName = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  item.textFieldValue = passwordDetails.website;
  item.textFieldEnabled = NO;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.hideIcon = YES;
  item.keyboardType = UIKeyboardTypeURL;
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_WEBSITE_PLACEHOLDER_TEXT);
  return item;
}

- (TableViewTextEditItem*)usernameItemForPasswordDetails:
    (PasswordDetails*)passwordDetails {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeUsername];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  item.textFieldValue = passwordDetails.username;  // Empty for a new form.
  // If password is missing (federated credential) don't allow to edit username.
  if (passwordDetails.credentialType != CredentialTypeFederation) {
    item.textFieldEnabled = self.tableView.editing;
    item.hideIcon = !self.tableView.editing;
    item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
    item.delegate = self;
  } else {
    item.textFieldEnabled = NO;
    item.hideIcon = YES;
  }
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_USERNAME_PLACEHOLDER_TEXT);
  return item;
}

- (TableViewTextEditItem*)passwordItemForPasswordDetails:
    (PasswordDetails*)passwordDetails {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypePassword];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  item.textFieldValue = [self isPasswordShown] || self.tableView.editing
                            ? passwordDetails.password
                            : kMaskedPassword;
  item.textFieldEnabled = self.tableView.editing;
  item.hideIcon = !self.tableView.editing;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.keyboardType = UIKeyboardTypeURL;
  item.returnKeyType = UIReturnKeyDone;
  item.delegate = self;
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_PASSWORD_PLACEHOLDER_TEXT);

  // During editing password is exposed so eye icon shouldn't be shown.
  if (!self.tableView.editing) {
    if (UseSymbols()) {
      UIImage* image =
          [self isPasswordShown]
              ? DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolSize)
              : DefaultSymbolWithPointSize(kShowActionSymbol, kSymbolSize);
      item.identifyingIcon = image;
    } else {
      NSString* image = [self isPasswordShown]
                            ? @"infobar_hide_password_icon"
                            : @"infobar_reveal_password_icon";
      item.identifyingIcon = [[UIImage imageNamed:image]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    }
    item.identifyingIconEnabled = YES;
    item.identifyingIconAccessibilityLabel = l10n_util::GetNSString(
        [self isPasswordShown] ? IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON
                               : IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
  }
  return item;
}

- (TableViewTextEditItem*)federationItemForPasswordDetails:
    (PasswordDetails*)passwordDetails {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc]   initWithType:ItemTypeFederation];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION);
  item.textFieldValue = passwordDetails.federation;
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
  if (UseSymbols()) {
    item.imageViewTintColor = [UIColor colorNamed:kRedColor];
  }
  return item;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
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
        int passwordIndex = IsPasswordGroupingEnabled() ? indexPath.section : 0;
        DCHECK(self.applicationCommandsHandler);
        DCHECK(self.passwords[passwordIndex].changePasswordURL.is_valid());
        OpenNewTabCommand* command = [OpenNewTabCommand
            commandWithURLFromChrome:self.passwords[passwordIndex]
                                         .changePasswordURL];
        UmaHistogramEnumeration("PasswordManager.BulkCheck.UserAction",
                                PasswordCheckInteraction::kChangePassword);
        [self.applicationCommandsHandler closeSettingsUIAndOpenURL:command];
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
  return !self.editing;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (sectionIdentifier == SectionIdentifierSite ||
      sectionIdentifier == SectionIdentifierPassword) {
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
      [textFieldCell.identifyingIconButton setTag:indexPath.section];
      break;
    }
    case ItemTypeWebsite:
    case ItemTypeFederation:
    case ItemTypeChangePasswordButton:
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
    case ItemTypeUsername:
    case ItemTypePassword:
      return YES;
  }
  return NO;
}

#pragma mark - PasswordDetailsConsumer

- (void)setPasswords:(NSArray<PasswordDetails*>*)passwords
            andTitle:(NSString*)title {
  if (IsPasswordGroupingEnabled()) {
    DCHECK(passwords.count > 0);
  } else {
    DCHECK(passwords.count == 1);
  }

  _passwords = passwords;
  _pageTitle = title;

  [self updateNavigationTitle];

  [self reloadData];
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  [self reconfigureCellsForItems:@[ tableViewItem ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  BOOL usernameValid = [self checkIfValidUsernames];
  BOOL passwordValid = [self checkIfValidPasswords];

  self.shouldEnableEditDoneButton = usernameValid && passwordValid;
  [self toggleNavigationBarRightButtonItem];
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  if ([tableViewItem.textFieldName
          isEqualToString:l10n_util::GetNSString(
                              IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD)]) {
    [self checkIfValidPasswords];
  }
  [self reconfigureCellsForItems:@[ tableViewItem ]];
}

#pragma mark - SettingsRootTableViewController

// Called when user tapped Delete button during editing. It means presented
// password should be deleted.
// TODO(crbug.com/1358988): Fix delete logic.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  // Remove this verification when it is implemented for password grouping.
  if (!IsPasswordGroupingEnabled()) {
    DCHECK(self.handler);
    // Pass origin only if password is present as confirmation message makes
    // sense only in this case.
    if ([self.passwords[0].password length]) {
      [self.handler
          showPasswordDeleteDialogWithOrigin:self.passwords[0].origin
                         compromisedPassword:self.passwords[0].isCompromised];
    } else {
      [self.handler showPasswordDeleteDialogWithOrigin:nil
                                   compromisedPassword:NO];
    }
  }
}

// TODO(crbug.com/1359392): Remove this override when kPasswordsGrouping flag is
// removed.
- (BOOL)shouldHideToolbar {
  // When credentials are grouped, each credential section has its own Delete
  // button displayed on editing mode, hence showing the toolbar with the Delete
  // button is not necessary.
  return IsPasswordGroupingEnabled() || !self.editing;
}

#pragma mark - Private

// Applies tint colour and resizes image.
- (UIImage*)compromisedIcon {
  if (UseSymbols()) {
    return DefaultSymbolTemplateWithPointSize(kWarningFillSymbol,
                                              kCompromisedPasswordSymbolSize);
  }
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
    void (^showPasswordHandler)(ReauthenticationResult) = ^(
        ReauthenticationResult result) {
      PasswordDetailsTableViewController* strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }
      [strongSelf logPasswordSettingsReauthResult:result];

      if (result == ReauthenticationResult::kFailure) {
        if (reason == ReauthenticationReasonCopy) {
          [strongSelf
               showToast:l10n_util::GetNSString(
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
      self.passwordDetailsInfoItems[_passwordIndexToReveal]
          .passwordTextItem.textFieldValue =
          self.passwords[_passwordIndexToReveal].password;
      if (UseSymbols()) {
        self.passwordDetailsInfoItems[_passwordIndexToReveal]
            .passwordTextItem.identifyingIcon =
            DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolSize);
      } else {
        self.passwordDetailsInfoItems[_passwordIndexToReveal]
            .passwordTextItem.identifyingIcon =
            [[UIImage imageNamed:@"infobar_hide_password_icon"]
                imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      }
      self.passwordDetailsInfoItems[_passwordIndexToReveal]
          .passwordTextItem.identifyingIconAccessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON);
      [self reconfigureCellsForItems:@[
        self.passwordDetailsInfoItems[_passwordIndexToReveal].passwordTextItem
      ]];
      if (self.passwords[_passwordIndexToReveal].compromised) {
        UmaHistogramEnumeration("PasswordManager.BulkCheck.UserAction",
                                PasswordCheckInteraction::kShowPassword);
      }
      break;
    case ReauthenticationReasonCopy: {
      UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];

      generalPasteboard.string =
          self.passwords[IsPasswordGroupingEnabled()
                             ? self.tableView.indexPathForSelectedRow.section
                             : 0]
              .password;
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
    case ItemTypeFederation:
    case ItemTypeChangePasswordButton:
    case ItemTypeChangePasswordRecommendation:
      return NO;
  }
}

// Checks if the usernames are valid and updates items accordingly.
- (BOOL)checkIfValidUsernames {
  DCHECK(self.passwords.count == self.passwordDetailsInfoItems.count);

  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    NSString* newUsernameValue =
        self.passwordDetailsInfoItems[i].usernameTextItem.textFieldValue;
    BOOL usernameChanged =
        ![newUsernameValue isEqualToString:self.passwords[i].username];
    BOOL showUsernameAlreadyUsed =
        usernameChanged &&
        [self.delegate isUsernameReused:newUsernameValue
                              forDomain:self.passwords[i].signonRealm];
    self.passwordDetailsInfoItems[i].usernameTextItem.hasValidText =
        !showUsernameAlreadyUsed;
    self.passwordDetailsInfoItems[i].usernameTextItem.identifyingIconEnabled =
        showUsernameAlreadyUsed;
    [self reconfigureCellsForItems:@[
      self.passwordDetailsInfoItems[i].usernameTextItem
    ]];

    if (showUsernameAlreadyUsed) {
      return NO;
    }
  }
  return YES;
}

// Checks if the passwords are valid and updates items accordingly.
- (BOOL)checkIfValidPasswords {
  DCHECK(self.passwords.count == self.passwordDetailsInfoItems.count);

  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    BOOL passwordEmpty = [self.passwordDetailsInfoItems[i]
                                 .passwordTextItem.textFieldValue length] == 0;
    self.passwordDetailsInfoItems[i].passwordTextItem.hasValidText =
        !passwordEmpty;
    [self reconfigureCellsForItems:@[
      self.passwordDetailsInfoItems[i].passwordTextItem
    ]];

    if (passwordEmpty) {
      return NO;
    }
  }
  return YES;
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

// Enables/Disables the right bar button item in the navigation bar.
- (void)toggleNavigationBarRightButtonItem {
  self.navigationItem.rightBarButtonItem.enabled =
      self.shouldEnableEditDoneButton;
}

- (BOOL)hasAtLeastOnePassword {
  for (PasswordDetails* passwordDetails in self.passwords) {
    if (passwordDetails.password.length > 0) {
      return YES;
    }
  }
  return NO;
}

- (BOOL)fieldsDidChange {
  DCHECK(self.passwords.count == self.passwordDetailsInfoItems.count);

  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    if (![self.passwords[i].password
            isEqualToString:self.passwordDetailsInfoItems[i]
                                .passwordTextItem.textFieldValue] ||
        ![self.passwords[i].username
            isEqualToString:self.passwordDetailsInfoItems[i]
                                .usernameTextItem.textFieldValue]) {
      return YES;
    }
  }
  return NO;
}

// Updates the title displayed in the navigation bar.
- (void)updateNavigationTitle {
  if (!self.pageTitle || self.pageTitle.length == 0) {
    self.pageTitle = self.passwords[0].origin;
  }
  _titleLabel.text = self.pageTitle;
}

// Creates the model items corresponding to a `PasswordDetails` and adds them to
// the `model`.
- (void)addPasswordDetailsToModel:(PasswordDetails*)passwordDetails {
  TableViewModel* model = self.tableViewModel;
  PasswordDetailsInfoItem* passwordItem =
      [[PasswordDetailsInfoItem alloc] init];

  NSInteger sectionForWebsite;
  NSInteger sectionForPassword;
  NSInteger sectionForCompromisedInfo;

  if (IsPasswordGroupingEnabled()) {
    // Password details are displayed in its own section when Grouping is
    // enabled.
    NSInteger nextSection =
        kSectionIdentifierEnumZero + [model numberOfSections];
    [model addSectionWithIdentifier:nextSection];

    sectionForWebsite = nextSection;
    sectionForPassword = nextSection;
    sectionForCompromisedInfo = nextSection;
  } else {
    // Password details fields are displayed in separate sections when Grouping
    // is not enabled.
    sectionForWebsite = SectionIdentifierSite;
    sectionForPassword = SectionIdentifierPassword;
    sectionForCompromisedInfo = SectionIdentifierCompromisedInfo;

    [model addSectionWithIdentifier:SectionIdentifierSite];
    [model addSectionWithIdentifier:SectionIdentifierPassword];
    if (passwordDetails.compromised) {
      [model addSectionWithIdentifier:SectionIdentifierCompromisedInfo];
    }
  }

  // Add sites to section.
  // TODO(crbug.com/1358982): Show multiple sites when Password Grouping is
  // enabled.
  passwordItem.websiteTextItem =
      [self websiteItemForPasswordDetails:passwordDetails];
  [model addItem:passwordItem.websiteTextItem
      toSectionWithIdentifier:sectionForWebsite];

  // Add username and password to section according to credential type.
  switch (passwordDetails.credentialType) {
    case CredentialTypeRegular: {
      passwordItem.usernameTextItem =
          [self usernameItemForPasswordDetails:passwordDetails];
      [model addItem:passwordItem.usernameTextItem
          toSectionWithIdentifier:sectionForPassword];

      passwordItem.passwordTextItem =
          [self passwordItemForPasswordDetails:passwordDetails];
      [model addItem:passwordItem.passwordTextItem
          toSectionWithIdentifier:sectionForPassword];

      if (passwordDetails.isCompromised) {
        if (base::FeatureList::IsEnabled(
                password_manager::features::
                    kIOSEnablePasswordManagerBrandingUpdate)) {
          [model addItem:[self changePasswordRecommendationItem]
              toSectionWithIdentifier:sectionForCompromisedInfo];

          if (passwordDetails.changePasswordURL.is_valid()) {
            [model addItem:[self changePasswordItem]
                toSectionWithIdentifier:sectionForCompromisedInfo];
          }
        } else {
          if (passwordDetails.changePasswordURL.is_valid()) {
            [model addItem:[self changePasswordItem]
                toSectionWithIdentifier:sectionForCompromisedInfo];
          }
          [model addItem:[self changePasswordRecommendationItem]
              toSectionWithIdentifier:sectionForCompromisedInfo];
        }
      }
      break;
    }
    case CredentialTypeFederation: {
      passwordItem.usernameTextItem =
          [self usernameItemForPasswordDetails:passwordDetails];
      [model addItem:passwordItem.usernameTextItem
          toSectionWithIdentifier:sectionForPassword];

      // Federated password forms don't have password value.
      [model addItem:[self federationItemForPasswordDetails:passwordDetails]
          toSectionWithIdentifier:sectionForPassword];
      break;
    }

    case CredentialTypeBlocked: {
      break;
    }
  }
  [self.passwordDetailsInfoItems addObject:passwordItem];
}

#pragma mark - Actions

// Called when the user tapped on the show/hide button near password.
- (void)didTapShowHideButton:(UIButton*)buttonView {
  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:NO];
  if (IsPasswordGroupingEnabled()) {
    _passwordIndexToReveal = [buttonView tag];
  }

  if (self.isPasswordShown) {
    self.passwordShown = NO;
    self.passwordDetailsInfoItems[_passwordIndexToReveal]
        .passwordTextItem.textFieldValue = kMaskedPassword;

    if (UseSymbols()) {
      self.passwordDetailsInfoItems[_passwordIndexToReveal]
          .passwordTextItem.identifyingIcon =
          DefaultSymbolWithPointSize(kShowActionSymbol, kSymbolSize);
    } else {
      self.passwordDetailsInfoItems[_passwordIndexToReveal]
          .passwordTextItem.identifyingIcon =
          [[UIImage imageNamed:@"infobar_reveal_password_icon"]
              imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    }
    self.passwordDetailsInfoItems[_passwordIndexToReveal]
        .passwordTextItem.identifyingIconAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
    [self reconfigureCellsForItems:@[
      self.passwordDetailsInfoItems[_passwordIndexToReveal].passwordTextItem
    ]];
  } else {
    [self attemptToShowPasswordFor:ReauthenticationReasonShow];
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
      generalPasteboard.string =
          self.passwords[IsPasswordGroupingEnabled()
                             ? self.tableView.indexPathForSelectedRow.section
                             : 0]
              .website;
      message =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE);
      break;
    case ItemTypeUsername:
      generalPasteboard.string =
          self.passwords[IsPasswordGroupingEnabled()
                             ? self.tableView.indexPathForSelectedRow.section
                             : 0]
              .username;
      message =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE);
      break;
    case ItemTypeFederation:
      generalPasteboard.string =
          self.passwords[IsPasswordGroupingEnabled()
                             ? self.tableView.indexPathForSelectedRow.section
                             : 0]
              .federation;
      [self logCopyPasswordDetailsFailure:NO];
      return;
    case ItemTypePassword:
      [self attemptToShowPasswordFor:ReauthenticationReasonCopy];
      [self logCopyPasswordDetailsFailure:NO];
      return;
  }

  if (message.length) {
    [self logCopyPasswordDetailsFailure:NO];
    [self showToast:message forSuccess:YES];
  } else {
    // TODO(crbug.com/1359331): There's a bug that is caused by `menu` being
    // nil, which leads to a nil message and a crash. Avoiding the crash and
    // logging for monitoring the issue. Since `menu` is an instance of
    // `UIMenuController` which is deprecated on iOS 16, this crash should go
    // away once we switch to `UIEditMenuInteraction`.
    [self logCopyPasswordDetailsFailure:YES];
  }
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

- (void)logCopyPasswordDetailsFailure:(BOOL)failure {
  base::UmaHistogramBoolean(
      "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", failure);
}

#pragma mark - Public

- (void)passwordEditingConfirmed {
  DCHECK(self.passwords.count == self.passwordDetailsInfoItems.count);
  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    NSString* oldUsername = self.passwords[i].username;
    NSString* oldPassword = self.passwords[i].password;
    self.passwords[i].username =
        self.passwordDetailsInfoItems[i].usernameTextItem.textFieldValue;
    self.passwords[i].password =
        self.passwordDetailsInfoItems[i].passwordTextItem.textFieldValue;
    [self.delegate passwordDetailsViewController:self
                          didEditPasswordDetails:self.passwords[i]
                                 withOldUsername:oldUsername
                                  andOldPassword:oldPassword];
    if (self.passwords[i].compromised) {
      UmaHistogramEnumeration("PasswordManager.BulkCheck.UserAction",
                              PasswordCheckInteraction::kEditPassword);
    }
  }
  [self.delegate didFinishEditingPasswordDetails];
  [super editButtonPressed];
  [self reloadData];
}

- (void)showEditViewWithoutAuthentication {
  self.showPasswordWithoutAuth = YES;
  [self editButtonPressed];
}

@end
