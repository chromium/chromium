// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller_presentation_delegate.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Section identifiers in the Incognito lock settings page table view.
enum SectionIdentifier {
  kLockStates = kSectionIdentifierEnumZero,
};

// Item identifiers in the Incognito lock settings page table view.
enum ItemIdentifier {
  kHeader = kItemTypeEnumZero,
  kDoNotHide,
  kHideWithSoftLock,
  kHideWithReauth,
};

}  // namespace

@implementation IncognitoLockViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.title = l10n_util::GetNSString(IDS_IOS_INCOGNITO_LOCK_SETTING_NAME);
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:kLockStates];

  TableViewLinkHeaderFooterItem* header =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:kHeader];
  header.text = l10n_util::GetNSString(IDS_IOS_INCOGNITO_LOCK_HEADER);
  [model setHeader:header forSectionWithIdentifier:kLockStates];

  TableViewTextItem* doNotHideItem =
      [[TableViewTextItem alloc] initWithType:kDoNotHide];
  doNotHideItem.text =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_LOCK_DO_NOT_HIDE);
  [model addItem:doNotHideItem toSectionWithIdentifier:kLockStates];

  TableViewDetailTextItem* hideWithSoftLockItem =
      [[TableViewDetailTextItem alloc] initWithType:kHideWithSoftLock];
  hideWithSoftLockItem.text =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_LOCK_HIDE_WITH_SOFT_LOCK_TITLE);
  hideWithSoftLockItem.detailText = l10n_util::GetNSString(
      IDS_IOS_INCOGNITO_LOCK_HIDE_WITH_SOFT_LOCK_DESCRIPTION);
  // Allow for text to wrap into multiple lines to fit the cell.
  hideWithSoftLockItem.allowMultilineDetailText = YES;
  [model addItem:hideWithSoftLockItem toSectionWithIdentifier:kLockStates];

  TableViewTextItem* hideWithReauthItem =
      [[TableViewTextItem alloc] initWithType:kHideWithReauth];
  hideWithReauthItem.text = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_LOCK_HIDE_WITH_REAUTH,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  [model addItem:hideWithReauthItem toSectionWithIdentifier:kLockStates];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.presentationDelegate incognitoLockViewControllerDidRemove:self];
  [super viewDidDisappear:animated];
}

#pragma mark - SettingsControllerProtocol

// Called when user dismissed settings. View controllers must implement this
// method and report dismissal User Action.
- (void)reportDismissalUserAction {
  // TODO(crbug.com/370804664): Report dismissal metric of the incognito setting
  // page.
}

// Called when user goes back from a settings view controller. View controllers
// must implement this method and report appropriate User Action.
- (void)reportBackUserAction {
  // TODO(crbug.com/370804664): Report back button metric from the incognito
  // setting page.
}

@end
