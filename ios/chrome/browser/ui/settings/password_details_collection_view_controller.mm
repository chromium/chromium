// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password_details_collection_view_controller.h"

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
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/settings/cells/password_details_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/ui/settings/reauthentication_module.h"
#import "ios/chrome/browser/ui/settings/save_passwords_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

// This protocol declares the methods used by the context menus, so that
// selectors can be created from those methods and passed to UIMenuItem.
@protocol PasswordDetailsViewerProtocol<NSObject>
- (void)copySite;
- (void)copyUsername;
- (void)copyPassword;
- (void)showPassword;
- (void)hidePassword;
@end

@interface PasswordDetailsCollectionViewController ()<
    PasswordDetailsViewerProtocol> {
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
  // Instance of the parent view controller needed in order to update the
  // password list when a password is deleted.
  __weak id<PasswordDetailsCollectionViewControllerDelegate> _weakDelegate;
  // Module containing the reauthentication mechanism for viewing and copying
  // passwords.
  __weak id<ReauthenticationProtocol> _weakReauthenticationModule;
  // The password item.
  PasswordDetailsItem* _passwordItem;
}

// Alert dialogue to confirm deletion of passwords upon pressing the delete
// button.
@property(nonatomic, strong) UIAlertController* deleteConfirmation;

@end

@implementation PasswordDetailsCollectionViewController

@synthesize deleteConfirmation = _deleteConfirmation;

- (instancetype)
  initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
              delegate:
                  (id<PasswordDetailsCollectionViewControllerDelegate>)delegate
reauthenticationModule:(id<ReauthenticationProtocol>)reauthenticationModule {
  DCHECK(delegate);
  DCHECK(reauthenticationModule);
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    _weakDelegate = delegate;
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
    _site = base::SysUTF8ToNSString(name_and_link.second.spec());
    self.title = [PasswordDetailsCollectionViewController
        simplifyOrigin:base::SysUTF8ToNSString(name_and_link.first)];
    self.collectionViewAccessibilityIdentifier =
        @"PasswordDetailsCollectionViewController";
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(hidePassword)
                          name:UIApplicationDidEnterBackgroundNotification
                        object:nil];

    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

+ (NSString*)simplifyOrigin:(NSString*)origin {
  NSString* originWithoutScheme = nil;
  if (![origin rangeOfString:@"://"].length) {
    originWithoutScheme = origin;
  } else {
    originWithoutScheme =
        [[origin componentsSeparatedByString:@"://"] objectAtIndex:1];
  }
  return
      [[originWithoutScheme componentsSeparatedByString:@"/"] objectAtIndex:0];
}

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSite];
  SettingsTextItem* siteHeader =
      [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
  siteHeader.text = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  siteHeader.textColor = [[MDCPalette greyPalette] tint500];
  [model setHeader:siteHeader forSectionWithIdentifier:SectionIdentifierSite];
  PasswordDetailsItem* siteItem =
      [[PasswordDetailsItem alloc] initWithType:ItemTypeSite];
  siteItem.text = _site;
  siteItem.showingText = YES;
  [model addItem:siteItem toSectionWithIdentifier:SectionIdentifierSite];
  [model addItem:[self siteCopyButtonItem]
      toSectionWithIdentifier:SectionIdentifierSite];

  if (!_passwordForm.blacklisted_by_user) {
    [model addSectionWithIdentifier:SectionIdentifierUsername];
    SettingsTextItem* usernameHeader =
        [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
    usernameHeader.text =
        l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
    usernameHeader.textColor = [[MDCPalette greyPalette] tint500];
    [model setHeader:usernameHeader
        forSectionWithIdentifier:SectionIdentifierUsername];
    PasswordDetailsItem* usernameItem =
        [[PasswordDetailsItem alloc] initWithType:ItemTypeUsername];
    usernameItem.text = _username;
    usernameItem.showingText = YES;
    [model addItem:usernameItem
        toSectionWithIdentifier:SectionIdentifierUsername];
    [model addItem:[self usernameCopyButtonItem]
        toSectionWithIdentifier:SectionIdentifierUsername];

    if (_passwordForm.federation_origin.opaque()) {
      [model addSectionWithIdentifier:SectionIdentifierPassword];
      SettingsTextItem* passwordHeader =
          [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
      passwordHeader.text =
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
      passwordHeader.textColor = [[MDCPalette greyPalette] tint500];
      [model setHeader:passwordHeader
          forSectionWithIdentifier:SectionIdentifierPassword];
      _passwordItem =
          [[PasswordDetailsItem alloc] initWithType:ItemTypePassword];
      _passwordItem.text = _password;
      _passwordItem.showingText = NO;
      [model addItem:_passwordItem
          toSectionWithIdentifier:SectionIdentifierPassword];

      [model addItem:[self passwordCopyButtonItem]
          toSectionWithIdentifier:SectionIdentifierPassword];
      [model addItem:[self showHidePasswordButtonItem]
          toSectionWithIdentifier:SectionIdentifierPassword];
    } else {
      [model addSectionWithIdentifier:SectionIdentifierFederation];
      SettingsTextItem* federationHeader =
          [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
      federationHeader.text =
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION);
      federationHeader.textColor = [[MDCPalette greyPalette] tint500];
      [model setHeader:federationHeader
          forSectionWithIdentifier:SectionIdentifierFederation];
      PasswordDetailsItem* federationItem =
          [[PasswordDetailsItem alloc] initWithType:ItemTypeFederation];
      federationItem.text = _federation;
      federationItem.showingText = YES;
      [model addItem:federationItem
          toSectionWithIdentifier:SectionIdentifierFederation];
    }
  }

  [model addSectionWithIdentifier:SectionIdentifierDelete];
  [model addItem:[self deletePasswordButtonItem]
      toSectionWithIdentifier:SectionIdentifierDelete];
}

#pragma mark - Items

- (CollectionViewItem*)siteCopyButtonItem {
  SettingsTextItem* item =
      [[SettingsTextItem alloc] initWithType:ItemTypeCopySite];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_COPY_BUTTON);
  item.textColor = [[MDCPalette cr_bluePalette] tint500];
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

- (CollectionViewItem*)usernameCopyButtonItem {
  SettingsTextItem* item =
      [[SettingsTextItem alloc] initWithType:ItemTypeCopyUsername];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON);
  item.textColor = [[MDCPalette cr_bluePalette] tint500];
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

- (CollectionViewItem*)passwordCopyButtonItem {
  SettingsTextItem* item =
      [[SettingsTextItem alloc] initWithType:ItemTypeCopyPassword];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON);
  item.textColor = [[MDCPalette cr_bluePalette] tint500];
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

- (CollectionViewItem*)showHidePasswordButtonItem {
  SettingsTextItem* item =
      [[SettingsTextItem alloc] initWithType:ItemTypeShowHide];
  item.text = [self showHideButtonText];
  item.textColor = [[MDCPalette cr_bluePalette] tint500];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

- (CollectionViewItem*)deletePasswordButtonItem {
  SettingsTextItem* item =
      [[SettingsTextItem alloc] initWithType:ItemTypeDelete];
  item.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON);
  item.textColor = [[MDCPalette cr_redPalette] tint500];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

#pragma mark - Actions

- (void)copySite {
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  generalPasteboard.string = _site;
  [self showToast:l10n_util::GetNSString(
                      IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE)
       forSuccess:YES];
}

- (void)copyUsername {
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  generalPasteboard.string = _username;
  [self showToast:l10n_util::GetNSString(
                      IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE)
       forSuccess:YES];
}

- (NSString*)showHideButtonText {
  if (_plainTextPasswordShown) {
    return l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON);
  }
  return l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
}

// Changes the text on the Show/Hide button appropriately according to
// |_plainTextPasswordShown|.
- (void)toggleShowHideButton {
  CollectionViewModel* model = self.collectionViewModel;
  NSIndexPath* path = [model indexPathForItemType:ItemTypeShowHide
                                sectionIdentifier:SectionIdentifierPassword];
  SettingsTextItem* item =
      base::mac::ObjCCastStrict<SettingsTextItem>([model itemAtIndexPath:path]);
  item.text = [self showHideButtonText];
  item.textColor = [[MDCPalette cr_bluePalette] tint500];
  [self reconfigureCellsForItems:@[ item ]];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (void)showPassword {
  if (_plainTextPasswordShown) {
    return;
  }

  if ([_weakReauthenticationModule canAttemptReauth]) {
    __weak PasswordDetailsCollectionViewController* weakSelf = self;
    void (^showPasswordHandler)(BOOL) = ^(BOOL success) {
      PasswordDetailsCollectionViewController* strongSelf = weakSelf;
      if (!strongSelf || !success)
        return;
      PasswordDetailsItem* passwordItem = strongSelf->_passwordItem;
      passwordItem.showingText = YES;
      [strongSelf reconfigureCellsForItems:@[ passwordItem ]];
      [[strongSelf collectionView].collectionViewLayout invalidateLayout];
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

- (void)hidePassword {
  if (!_plainTextPasswordShown) {
    return;
  }
  _passwordItem.showingText = NO;
  [self reconfigureCellsForItems:@[ _passwordItem ]];
  [self.collectionView.collectionViewLayout invalidateLayout];
  _plainTextPasswordShown = NO;
  [self toggleShowHideButton];
}

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
    __weak PasswordDetailsCollectionViewController* weakSelf = self;
    void (^copyPasswordHandler)(BOOL) = ^(BOOL success) {
      PasswordDetailsCollectionViewController* strongSelf = weakSelf;
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
  [MDCSnackbarManager showMessage:copyPasswordResultMessage];
}

- (void)deletePassword {
  __weak PasswordDetailsCollectionViewController* weakSelf = self;

  self.deleteConfirmation = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

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
                PasswordDetailsCollectionViewController* strongSelf = weakSelf;
                if (!strongSelf) {
                  return;
                }
                strongSelf.deleteConfirmation = nil;
                [strongSelf->_weakDelegate deletePassword:_passwordForm];
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
                           collectionView:(UICollectionView*)collectionView
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
    UICollectionViewLayoutAttributes* attributes =
        [collectionView.collectionViewLayout
            layoutAttributesForItemAtIndexPath:indexPath];
    [menu setTargetRect:attributes.frame inView:collectionView];
    [menu setMenuVisible:YES animated:YES];
  }
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSite:
    case ItemTypeUsername:
    case ItemTypePassword:
      [self ensureContextMenuShownForItemType:itemType
                               collectionView:collectionView
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
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeSite:
    case ItemTypeUsername:
    case ItemTypePassword:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    default:
      return MDCCellDefaultOneLineHeight;
  }
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
