// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details_table_view_controller.h"

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication_module.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPasswordDetailsTableViewId = @"PasswordDetailsTableViewId";
NSString* const kPasswordDetailsDeletionAlertViewId =
    @"PasswordDetailsDeletionAlertViewId";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSite = kSectionIdentifierEnumZero,
  SectionIdentifierUsername,
  SectionIdentifierPassword,
  SectionIdentifierFederation,
  SectionIdentifierDelete,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeSite,
  ItemTypeCopySite,
  ItemTypeUsername,
  ItemTypeCopyUsername,
  ItemTypePassword,
  ItemTypeCopyPassword,
  ItemTypeShowHide,
  ItemTypeFederation,
  ItemTypeDelete,
};

}  // namespace

@interface PasswordDetailsTableViewController () {
  // The username to which the saved password belongs.
  NSString* _username;
  // The saved password.
  NSString* _password;
  // The federation providing this credential, if any.
  NSString* _federation;
  // The origin site of the saved credential.
  NSString* _site;
  // Whether the password is shown in plain text form or in obscured form.
  BOOL _plainTextPasswordShown;
  // The password form.
  autofill::PasswordForm _passwordForm;
  // Module containing the reauthentication mechanism for viewing and copying
  // passwords.
  __weak id<ReauthenticationProtocol> _weakReauthenticationModule;
  // The password item.
  TableViewTextItem* _passwordItem;
}

// Alert dialog to confirm deletion of passwords upon pressing the delete
// button.
@property(nonatomic, strong) UIAlertController* deleteConfirmation;

// Instance of the parent view controller needed in order to update the
// password list when a password is deleted.
@property(nonatomic, weak) id<PasswordDetailsTableViewControllerDelegate>
    delegate;

@end

@implementation PasswordDetailsTableViewController

@synthesize deleteConfirmation = _deleteConfirmation;

- (instancetype)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
                            delegate:
                                (id<PasswordDetailsTableViewControllerDelegate>)
                                    delegate
              reauthenticationModule:
                  (id<ReauthenticationProtocol>)reauthenticationModule {
  DCHECK(delegate);
  DCHECK(reauthenticationModule);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _delegate = delegate;
    _weakReauthenticationModule = reauthenticationModule;

    _passwordForm = passwordForm;
    if (!_passwordForm.blacklisted_by_user) {
      _username = base::SysUTF16ToNSString(_passwordForm.username_value);
      if (_passwordForm.federation_origin.opaque()) {
        _password = base::SysUTF16ToNSString(_passwordForm.password_value);
      } else {
        _federation =
            base::SysUTF8ToNSString(_passwordForm.federation_origin.host());
      }
    }
    auto name_and_link =
        password_manager::GetShownOriginAndLinkUrl(_passwordForm);
    self.title = base::SysUTF8ToNSString(name_and_link.first);
    _site = base::SysUTF8ToNSString(name_and_link.second.spec());
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(hidePassword)
                          name:UIApplicationDidEnterBackgroundNotification
                        object:nil];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPasswordDetailsTableViewId;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSite];
  TableViewTextHeaderFooterItem* siteHeader =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  siteHeader.text = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  [model setHeader:siteHeader forSectionWithIdentifier:SectionIdentifierSite];
  TableViewTextItem* siteItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeSite];
  siteItem.text = _site;
  [model addItem:siteItem toSectionWithIdentifier:SectionIdentifierSite];
  [model addItem:[self siteCopyButtonItem]
      toSectionWithIdentifier:SectionIdentifierSite];

  if (!_passwordForm.blacklisted_by_user) {
    [model addSectionWithIdentifier:SectionIdentifierUsername];
    TableViewTextHeaderFooterItem* usernameHeader =
        [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
    usernameHeader.text =
        l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
    [model setHeader:usernameHeader
        forSectionWithIdentifier:SectionIdentifierUsername];
    TableViewTextItem* usernameItem =
        [[TableViewTextItem alloc] initWithType:ItemTypeUsername];
    usernameItem.text = _username;
    [model addItem:usernameItem
        toSectionWithIdentifier:SectionIdentifierUsername];
    [model addItem:[self usernameCopyButtonItem]
        toSectionWithIdentifier:SectionIdentifierUsername];

    if (_passwordForm.federation_origin.opaque()) {
      [model addSectionWithIdentifier:SectionIdentifierPassword];
      TableViewTextHeaderFooterItem* passwordHeader =
          [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
      passwordHeader.text =
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
      [model setHeader:passwordHeader
          forSectionWithIdentifier:SectionIdentifierPassword];
      _passwordItem = [[TableViewTextItem alloc] initWithType:ItemTypePassword];
      _passwordItem.text = _password;
      _passwordItem.masked = YES;
      [model addItem:_passwordItem
          toSectionWithIdentifier:SectionIdentifierPassword];

      [model addItem:[self passwordCopyButtonItem]
          toSectionWithIdentifier:SectionIdentifierPassword];
      [model addItem:[self showHidePasswordButtonItem]
          toSectionWithIdentifier:SectionIdentifierPassword];
    } else {
      [model addSectionWithIdentifier:SectionIdentifierFederation];
      TableViewTextHeaderFooterItem* federationHeader =
          [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
      federationHeader.text =
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION);
      [model setHeader:federationHeader
          forSectionWithIdentifier:SectionIdentifierFederation];
      TableViewTextItem* federationItem =
          [[TableViewTextItem alloc] initWithType:ItemTypeFederation];
      federationItem.text = _federation;
      [model addItem:federationItem
          toSectionWithIdentifier:SectionIdentifierFederation];
    }
  }

  [model addSectionWithIdentifier:SectionIdentifierDelete];
  [model addItem:[self deletePasswordButtonItem]
      toSectionWithIdentifier:SectionIdentifierDelete];
}

#pragma mark - Items

// Returns the "Copy" button for site.
- (TableViewItem*)siteCopyButtonItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeCopySite];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_COPY_BUTTON);
  item.textColor = [UIColor colorNamed:kBlueColor];
  // Accessibility label adds the header to the text, so that accessibility
  // users do not have to rely on the visual grouping to understand which part
  // of the credential is being copied.
  item.accessibilityLabel = [NSString
      stringWithFormat:@"%@: %@",
                       l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE),
                       l10n_util::GetNSString(
                           IDS_IOS_SETTINGS_SITE_COPY_BUTTON)];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

// Returns the "Copy" button for user name.
- (TableViewItem*)usernameCopyButtonItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeCopyUsername];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON);
  item.textColor = [UIColor colorNamed:kBlueColor];
  // Accessibility label adds the header to the text, so that accessibility
  // users do not have to rely on the visual grouping to understand which part
  // of the credential is being copied.
  item.accessibilityLabel =
      [NSString stringWithFormat:@"%@: %@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME),
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON)];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

// Returns the "Copy" button for password.
- (TableViewItem*)passwordCopyButtonItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeCopyPassword];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON);
  item.textColor = [UIColor colorNamed:kBlueColor];
  // Accessibility label adds the header to the text, so that accessibility
  // users do not have to rely on the visual grouping to understand which part
  // of the credential is being copied.
  item.accessibilityLabel =
      [NSString stringWithFormat:@"%@: %@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD),
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON)];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

// Returns the "Show/Hide" toggle button for password.
- (TableViewItem*)showHidePasswordButtonItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeShowHide];
  item.text = [self showHideButtonText];
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

// Returns the "Delete" button for password.
- (TableViewItem*)deletePasswordButtonItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeDelete];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON);
  item.textColor = [UIColor colorNamed:kRedColor];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

#pragma mark - Actions

// Copies the site to system pasteboard and shows a toast of success.
- (void)copySite {
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  generalPasteboard.string = _site;
  [self showToast:l10n_util::GetNSString(
                      IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE)
       forSuccess:YES];
}

// Copies the user name to system pasteboard and shows a toast of success.
- (void)copyUsername {
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  generalPasteboard.string = _username;
  [self showToast:l10n_util::GetNSString(
                      IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE)
       forSuccess:YES];
}

// Returns the title of "Show/Hide" toggle button for password based on current
// state of |_plainTextPasswordShown|.
- (NSString*)showHideButtonText {
  if (_plainTextPasswordShown) {
    return l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON);
  }
  return l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
}

// Changes the text on the Show/Hide button appropriately according to
// |_plainTextPasswordShown|.
- (void)toggleShowHideButton {
  TableViewModel* model = self.tableViewModel;
  NSIndexPath* path = [model indexPathForItemType:ItemTypeShowHide
                                sectionIdentifier:SectionIdentifierPassword];
  TableViewTextItem* item = base::mac::ObjCCastStrict<TableViewTextItem>(
      [model itemAtIndexPath:path]);
  item.text = [self showHideButtonText];
  [self reconfigureCellsForItems:@[ item ]];
}

// Shows the password and toggles the "Show/Hide" button if the user can
// reauthenticate, otherwise shows the password dialog.
- (void)showPassword {
  if (_plainTextPasswordShown) {
    return;
  }

  if ([_weakReauthenticationModule canAttemptReauth]) {
    __weak PasswordDetailsTableViewController* weakSelf = self;
    void (^showPasswordHandler)(BOOL) = ^(BOOL success) {
      PasswordDetailsTableViewController* strongSelf = weakSelf;
      if (!strongSelf || !success)
        return;
      TableViewTextItem* passwordItem = strongSelf->_passwordItem;
      passwordItem.masked = NO;
      [strongSelf reconfigureCellsForItems:@[ passwordItem ]];
      strongSelf->_plainTextPasswordShown = YES;
      [strongSelf toggleShowHideButton];
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AccessPasswordInSettings",
          password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
          password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
    };

    [_weakReauthenticationModule
        attemptReauthWithLocalizedReason:
            l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW)
                    canReusePreviousAuth:YES
                                 handler:showPasswordHandler];
  } else {
    [self showPasscodeDialog];
  }
}

// Hides the password and toggles the "Show/Hide" button.
- (void)hidePassword {
  if (!_plainTextPasswordShown) {
    return;
  }
  _passwordItem.masked = YES;
  [self reconfigureCellsForItems:@[ _passwordItem ]];
  _plainTextPasswordShown = NO;
  [self toggleShowHideButton];
}

// Copies the password to system pasteboard and shows a toast of success if the
// user can reauthenticate, otherwise shows the password dialog.
- (void)copyPassword {
  // If the password is displayed in plain text, there is no need to
  // re-authenticate the user when copying the password because they are already
  // granted access to it.
  if (_plainTextPasswordShown) {
    UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
    generalPasteboard.string = _password;
    [self showToast:l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE)
         forSuccess:YES];
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.AccessPasswordInSettings",
        password_manager::metrics_util::ACCESS_PASSWORD_COPIED,
        password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.ReauthToAccessPasswordInSettings",
        password_manager::metrics_util::REAUTH_SKIPPED,
        password_manager::metrics_util::REAUTH_COUNT);
  } else if ([_weakReauthenticationModule canAttemptReauth]) {
    __weak PasswordDetailsTableViewController* weakSelf = self;
    void (^copyPasswordHandler)(BOOL) = ^(BOOL success) {
      PasswordDetailsTableViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      if (success) {
        UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
        generalPasteboard.string = strongSelf->_password;
        [strongSelf showToast:l10n_util::GetNSString(
                                  IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE)
                   forSuccess:YES];
        UMA_HISTOGRAM_ENUMERATION(
            "PasswordManager.AccessPasswordInSettings",
            password_manager::metrics_util::ACCESS_PASSWORD_COPIED,
            password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
      } else {
        [strongSelf
             showToast:l10n_util::GetNSString(
                           IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE)
            forSuccess:NO];
      }
    };
    [_weakReauthenticationModule
        attemptReauthWithLocalizedReason:
            l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_COPY)
                    canReusePreviousAuth:YES
                                 handler:copyPasswordHandler];
  } else {
    [self showPasscodeDialog];
  }
}

// Show a dialog offering the user to set a passcode in order to see the
// passwords.
- (void)showPasscodeDialog {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE)
                       message:l10n_util::GetNSString(
                                   IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_CONTENT)
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

// Show a MD snack bar with |message| and provide haptic feedback. The haptic
// feedback is either for success or for error, depending on |success|.
- (void)showToast:(NSString*)message forSuccess:(BOOL)success {
  TriggerHapticFeedbackForNotification(success
                                           ? UINotificationFeedbackTypeSuccess
                                           : UINotificationFeedbackTypeError);
  MDCSnackbarMessage* copyPasswordResultMessage =
      [MDCSnackbarMessage messageWithText:message];
  copyPasswordResultMessage.category = @"PasswordsSnackbarCategory";
  [self.dispatcher showSnackbarMessage:copyPasswordResultMessage
                          bottomOffset:0];
}

// Deletes the password with a deletion confirmation alert.
- (void)deletePassword {
  __weak PasswordDetailsTableViewController* weakSelf = self;

  self.deleteConfirmation = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  _deleteConfirmation.view.accessibilityIdentifier =
      kPasswordDetailsDeletionAlertViewId;

  UIAlertAction* cancelAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CANCEL_PASSWORD_DELETION)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                weakSelf.deleteConfirmation = nil;
              }];
  [_deleteConfirmation addAction:cancelAction];

  UIAlertAction* deleteAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_DELETION)
                style:UIAlertActionStyleDestructive
              handler:^(UIAlertAction* action) {
                weakSelf.deleteConfirmation = nil;
                [weakSelf.delegate
                    passwordDetailsTableViewController:weakSelf
                                        deletePassword:_passwordForm];
              }];
  [_deleteConfirmation addAction:deleteAction];

  [self presentViewController:_deleteConfirmation animated:YES completion:nil];
}

// Returns an array of UIMenuItems to display in a context menu on the site
// cell.
- (NSArray*)getSiteMenuItems {
  UIMenuItem* copyOption = [[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_COPY_MENU_ITEM)
             action:@selector(copySite)];
  return @[ copyOption ];
}

// Returns an array of UIMenuItems to display in a context menu on the username
// cell.
- (NSArray*)getUsernameMenuItems {
  UIMenuItem* copyOption = [[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_USERNAME_COPY_MENU_ITEM)
             action:@selector(copyUsername)];
  return @[ copyOption ];
}

// Returns an array of UIMenuItems to display in a context menu on the password
// cell.
- (NSArray*)getPasswordMenuItems {
  UIMenuItem* copyOption = [[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_COPY_MENU_ITEM)
             action:@selector(copyPassword)];
  UIMenuItem* showOption = [[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_SHOW_MENU_ITEM)
             action:@selector(showPassword)];
  UIMenuItem* hideOption = [[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_HIDE_MENU_ITEM)
             action:@selector(hidePassword)];
  return @[ copyOption, showOption, hideOption ];
}

// If the context menu is not shown for a given item type, constructs that
// menu and shows it. This method should only be called for item types
// representing the cells with the site, username and password.
- (void)ensureContextMenuShownForItemType:(NSInteger)itemType
                                tableView:(UITableView*)tableView
                              atIndexPath:(NSIndexPath*)indexPath {
  UIMenuController* menu = [UIMenuController sharedMenuController];
  if (![menu isMenuVisible]) {
    NSArray* options = nil;
    switch (itemType) {
      case ItemTypeSite:
        options = [self getSiteMenuItems];
        break;
      case ItemTypeUsername:
        options = [self getUsernameMenuItems];
        break;
      case ItemTypePassword:
        options = [self getPasswordMenuItems];
        break;
      default:
        NOTREACHED();
    }
    [menu setMenuItems:options];
    [menu setTargetRect:[tableView rectForRowAtIndexPath:indexPath]
                 inView:tableView];
    [menu setMenuVisible:YES animated:YES];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSite:
    case ItemTypeUsername:
    case ItemTypePassword:
      [self ensureContextMenuShownForItemType:itemType
                                    tableView:tableView
                                  atIndexPath:indexPath];
      break;
    case ItemTypeCopySite:
      [self copySite];
      break;
    case ItemTypeCopyUsername:
      [self copyUsername];
      break;
    case ItemTypeShowHide:
      if (_plainTextPasswordShown) {
        [self hidePassword];
      } else {
        [self showPassword];
      }
      break;
    case ItemTypeCopyPassword:
      [self copyPassword];
      break;
    case ItemTypeDelete:
      [self deletePassword];
      break;
    default:
      break;
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - ForTesting

- (void)setReauthenticationModule:
    (id<ReauthenticationProtocol>)reauthenticationModule {
  _weakReauthenticationModule = reauthenticationModule;
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(copySite) || action == @selector(copyUsername) ||
      action == @selector(copyPassword) ||
      (_plainTextPasswordShown && action == @selector(hidePassword)) ||
      (!_plainTextPasswordShown && action == @selector(showPassword))) {
    return YES;
  }
  return NO;
}

@end
