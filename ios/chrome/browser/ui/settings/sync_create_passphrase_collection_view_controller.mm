// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync_create_passphrase_collection_view_controller.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/cells/byo_textfield_item.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using namespace sync_encryption_passphrase;

@interface SyncCreatePassphraseCollectionViewController () {
  UITextField* confirmPassphrase_;
}
// Returns a confirm passphrase item.
- (CollectionViewItem*)confirmPassphraseItem;
@end

@implementation SyncCreatePassphraseCollectionViewController

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

#pragma mark - View lifecycle

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

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  NSInteger enterPassphraseIndex =
      [model indexPathForItemType:ItemTypeEnterPassphrase
                sectionIdentifier:SectionIdentifierPassphrase]
          .item;
  [model insertItem:[self confirmPassphraseItem]
      inSectionWithIdentifier:SectionIdentifierPassphrase
                      atIndex:enterPassphraseIndex + 1];
}

#pragma mark - Items

- (CollectionViewItem*)confirmPassphraseItem {
  if (!confirmPassphrase_) {
    confirmPassphrase_ = [[UITextField alloc] init];
    [confirmPassphrase_ setSecureTextEntry:YES];
    [confirmPassphrase_ setBackgroundColor:[UIColor clearColor]];
    [confirmPassphrase_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    [confirmPassphrase_ setAutocorrectionType:UITextAutocorrectionTypeNo];
    [confirmPassphrase_
        setPlaceholder:l10n_util::GetNSString(
                           IDS_IOS_SYNC_CONFIRM_PASSPHRASE_LABEL)];
    [self registerTextField:confirmPassphrase_];
  }

  BYOTextFieldItem* item =
      [[BYOTextFieldItem alloc] initWithType:ItemTypeConfirmPassphrase];
  item.textField = confirmPassphrase_;
  return item;
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypeConfirmPassphrase) {
    [confirmPassphrase_ becomeFirstResponder];
  }
}

#pragma mark SyncEncryptionPassphraseCollectionViewController

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

@implementation SyncCreatePassphraseCollectionViewController (UsedForTesting)

- (UITextField*)confirmPassphrase {
  return confirmPassphrase_;
}

@end
