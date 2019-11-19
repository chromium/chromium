// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_create_passphrase_table_view_controller.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/settings/cells/byo_textfield_item.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using sync_encryption_passphrase::ItemTypeConfirmPassphrase;
using sync_encryption_passphrase::ItemTypeEnterPassphrase;
using sync_encryption_passphrase::SectionIdentifierPassphrase;

@interface SyncCreatePassphraseTableViewController () {
  UITextField* confirmPassphrase_;
}
// Returns a confirm passphrase item.
- (TableViewItem*)confirmPassphraseItem;
@end

@implementation SyncCreatePassphraseTableViewController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  self = [super initWithBrowserState:browserState];
  if (self) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_CREATE_PASSPHRASE);
    self.headerMessage = nil;
    self.footerMessage =
        l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_PASSPHRASE_INFO),
    self.processingMessage =
        l10n_util::GetNSString(IDS_IOS_SYNC_PASSPHRASE_ENCRYPTING);

    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

#pragma mark - UIViewController

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  if (![self isViewLoaded]) {
    confirmPassphrase_ = nil;
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if ([self isMovingFromParentViewController]) {
    [self unregisterTextField:self.confirmPassphrase];
  }
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  NSInteger enterPassphraseIndex =
      [model indexPathForItemType:ItemTypeEnterPassphrase
                sectionIdentifier:SectionIdentifierPassphrase]
          .item;
  [model insertItem:[self confirmPassphraseItem]
      inSectionWithIdentifier:SectionIdentifierPassphrase
                      atIndex:enterPassphraseIndex + 1];
}

#pragma mark - Items

- (TableViewItem*)confirmPassphraseItem {
  if (!confirmPassphrase_) {
    confirmPassphrase_ = [[UITextField alloc] init];
    confirmPassphrase_.secureTextEntry = YES;
    confirmPassphrase_.backgroundColor = UIColor.clearColor;
    confirmPassphrase_.autocorrectionType = UITextAutocorrectionTypeNo;
    confirmPassphrase_.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    confirmPassphrase_.adjustsFontForContentSizeCategory = YES;
    confirmPassphrase_.placeholder =
        l10n_util::GetNSString(IDS_IOS_SYNC_CONFIRM_PASSPHRASE_LABEL);
    [self registerTextField:confirmPassphrase_];
  }

  BYOTextFieldItem* item =
      [[BYOTextFieldItem alloc] initWithType:ItemTypeConfirmPassphrase];
  item.textField = confirmPassphrase_;
  return item;
}

#pragma mark UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypeConfirmPassphrase) {
    [confirmPassphrase_ becomeFirstResponder];
  }
}

#pragma mark SyncEncryptionPassphraseTableViewController

- (BOOL)forDecryption {
  return NO;
}

- (void)signInPressed {
  NSString* passphraseText = [self.passphrase text];
  NSString* confirmPassphraseText = [confirmPassphrase_ text];
  if (![self areAllFieldsFilled]) {
    [self clearFieldsOnError:l10n_util::GetNSString(
                                 IDS_SYNC_EMPTY_PASSPHRASE_ERROR)];
    [self reloadData];
    return;
  } else if (![passphraseText isEqualToString:confirmPassphraseText]) {
    [self clearFieldsOnError:l10n_util::GetNSString(
                                 IDS_SYNC_PASSPHRASE_MISMATCH_ERROR)];
    [self reloadData];
    return;
  }

  [super signInPressed];
}

- (BOOL)areAllFieldsFilled {
  return [super areAllFieldsFilled] && [self.confirmPassphrase text].length > 0;
}

- (void)clearFieldsOnError:(NSString*)errorMessage {
  [super clearFieldsOnError:errorMessage];
  [self.confirmPassphrase setText:@""];
}

#pragma mark - UIControl event listener

- (void)textFieldDidEndEditing:(id)sender {
  if (sender == self.passphrase) {
    [confirmPassphrase_ becomeFirstResponder];
  } else if (sender == self.confirmPassphrase) {
    if ([self areAllFieldsFilled]) {
      // The right nav bar button is disabled when either of the text fields is
      // empty.  Hitting return when a text field is empty should not cause the
      // password to be applied.
      [self signInPressed];
    } else {
      [self clearFieldsOnError:l10n_util::GetNSString(
                                   IDS_SYNC_EMPTY_PASSPHRASE_ERROR)];
      [self reloadData];
    }
  }
}

@end

@implementation SyncCreatePassphraseTableViewController (UsedForTesting)

- (UITextField*)confirmPassphrase {
  return confirmPassphrase_;
}

@end
