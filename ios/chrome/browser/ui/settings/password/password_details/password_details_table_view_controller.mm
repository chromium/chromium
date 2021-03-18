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
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_menu_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
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
  SectionIdentifierCompromisedInfo
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

}  // namespace

@interface PasswordDetailsTableViewController () <TableViewTextEditItemDelegate>

// Password which is shown on the screen.
@property(nonatomic, strong) PasswordDetails* password;

// Whether the password is shown in plain text form or in masked form.
@property(nonatomic, assign, getter=isPasswordShown) BOOL passwordShown;

// The text item related to the username value.
@property(nonatomic, strong) TableViewTextEditItem* usernameTextItem;

// The text item related to the password value.
@property(nonatomic, strong) TableViewTextEditItem* passwordTextItem;

// The view used to anchor error alert which is shown for the username. This is
// image icon in the |usernameTextItem| cell.
@property(nonatomic, weak) UIView* usernameErrorAnchorView;

@end

@implementation PasswordDetailsTableViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPasswordDetailsViewControllerId;
  self.tableView.allowsSelectionDuringEditing = YES;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.lineBreakMode = NSLineBreakByTruncatingHead;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.text = self.password.origin;
  self.navigationItem.titleView = titleLabel;
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.handler passwordDetailsTableViewControllerDidDisappear];
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
    // If password or username value was changed show confirmation dialog before
    // saving password. Editing mode will be exited only if user confirm saving.
    if (![self.password.password
            isEqualToString:self.passwordTextItem.textFieldValue] ||
        ![self.password.username
            isEqualToString:self.usernameTextItem.textFieldValue]) {
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
  [model addSectionWithIdentifier:SectionIdentifierPassword];

  [model addItem:[self websiteItem]
      toSectionWithIdentifier:SectionIdentifierPassword];

  // Blocked password forms have username equal to nil.
  if (self.password.username != nil) {
    self.usernameTextItem = [self usernameItem];
    [model addItem:self.usernameTextItem
        toSectionWithIdentifier:SectionIdentifierPassword];
  }

  // Federated and blocked password forms don't have password value.
  if ([self.password.password length]) {
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
  } else if ([self.password.federation length]) {
    [model addItem:[self federationItem]
        toSectionWithIdentifier:SectionIdentifierPassword];
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
  item.textFieldValue = self.password.website;
  item.textFieldEnabled = NO;
  item.hideIcon = YES;
  return item;
}

- (TableViewTextEditItem*)usernameItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeUsername];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  item.textFieldValue = self.password.username;
  // If password is missing (federated credential) don't allow to edit username.
  if ([self.password.password length] &&
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
  return item;
}

- (TableViewTextEditItem*)passwordItem {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:ItemTypePassword];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  item.textFieldValue = [self isPasswordShown] || self.tableView.editing
                            ? self.password.password
                            : kMaskedPassword;
  item.textFieldEnabled = self.tableView.editing;
  item.hideIcon = !self.tableView.editing;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.keyboardType = UIKeyboardTypeURL;
  item.returnKeyType = UIReturnKeyDone;
  item.delegate = self;

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
  item.textColor = self.tableView.editing ? UIColor.cr_secondaryLabelColor
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

#if !defined(__IPHONE_13_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_13_0
    [menu setTargetRect:[tableView rectForRowAtIndexPath:indexPath]
                 inView:tableView];
    [menu setMenuVisible:YES animated:YES];
#else
    [menu showMenuFromView:tableView
                      rect:[tableView rectForRowAtIndexPath:indexPath]];
#endif
  }
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  return !self.editing;
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

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  [self reconfigureCellsForItems:@[
    self.usernameTextItem, self.passwordTextItem
  ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  BOOL isInputValid = [self checkIfValidUsername] & [self checkIfValidPassword];
  self.navigationItem.rightBarButtonItem.enabled = isInputValid;
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  // Check if the item is equal to the current username or password item as when
  // editing finished reloadData is called.
  if (tableViewItem == self.usernameTextItem) {
    [self reconfigureCellsForItems:@[ self.usernameTextItem ]];
  } else if (tableViewItem == self.passwordTextItem) {
    [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
  }
}

#pragma mark - SettingsRootTableViewController

// Called when user tapped Delete button during editing. It means presented
// password should be deleted.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
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
  [UIColor.cr_secondaryLabelColor set];
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
      return NO;
  }
}

// Checks if the username is valid and updates item accordingly.
- (BOOL)checkIfValidUsername {
  DCHECK(self.password.username);
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
  DCHECK(self.password.password);

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
    self.passwordTextItem.textFieldValue = kMaskedPassword;
    self.passwordTextItem.identifyingIcon =
        [[UIImage imageNamed:@"infobar_reveal_password_icon"]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [self reconfigureCellsForItems:@[ self.passwordTextItem ]];
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
  }
}

@end
