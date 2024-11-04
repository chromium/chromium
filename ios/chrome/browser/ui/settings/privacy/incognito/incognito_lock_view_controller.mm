// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_mutator.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
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

// Converts an ItemIdentifier, to a corresponding IncognitoLockState.
IncognitoLockState StateWithItemIdentifier(ItemIdentifier item_identifier) {
  switch (item_identifier) {
    case ItemIdentifier::kDoNotHide:
      return IncognitoLockState::kNone;
    case ItemIdentifier::kHideWithSoftLock:
      return IncognitoLockState::kSoftLock;
    case ItemIdentifier::kHideWithReauth:
      return IncognitoLockState::kReauth;
    case ItemIdentifier::kHeader:
      NOTREACHED();
  }
}

}  // namespace

@implementation IncognitoLockViewController {
  IncognitoLockState _selectedState;
}

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

  TableViewTextItem* hideWithReauthItem =
      [[TableViewTextItem alloc] initWithType:kHideWithReauth];
  hideWithReauthItem.text = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_LOCK_HIDE_WITH_REAUTH,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  hideWithReauthItem.accessoryType =
      _selectedState == IncognitoLockState::kReauth
          ? UITableViewCellAccessoryCheckmark
          : UITableViewCellAccessoryNone;
  hideWithReauthItem.accessibilityTraits |= UIAccessibilityTraitButton;
  hideWithReauthItem.accessibilityIdentifier =
      kSettingsIncognitoLockHideWithReauthCellId;
  [model addItem:hideWithReauthItem toSectionWithIdentifier:kLockStates];

  TableViewDetailTextItem* hideWithSoftLockItem =
      [[TableViewDetailTextItem alloc] initWithType:kHideWithSoftLock];
  hideWithSoftLockItem.text =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_LOCK_HIDE_WITH_SOFT_LOCK_TITLE);
  hideWithSoftLockItem.detailText = l10n_util::GetNSString(
      IDS_IOS_INCOGNITO_LOCK_HIDE_WITH_SOFT_LOCK_DESCRIPTION);
  // Allow for text to wrap into multiple lines to fit the cell.
  hideWithSoftLockItem.allowMultilineDetailText = YES;
  hideWithSoftLockItem.accessoryType =
      _selectedState == IncognitoLockState::kSoftLock
          ? UITableViewCellAccessoryCheckmark
          : UITableViewCellAccessoryNone;
  hideWithSoftLockItem.accessibilityTraits |= UIAccessibilityTraitButton;
  hideWithSoftLockItem.accessibilityIdentifier =
      kSettingsIncognitoLockHideWithSoftLockCellId;
  [model addItem:hideWithSoftLockItem toSectionWithIdentifier:kLockStates];

  TableViewTextItem* doNotHideItem =
      [[TableViewTextItem alloc] initWithType:kDoNotHide];
  doNotHideItem.text =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_LOCK_DO_NOT_HIDE);
  doNotHideItem.accessoryType = _selectedState == IncognitoLockState::kNone
                                    ? UITableViewCellAccessoryCheckmark
                                    : UITableViewCellAccessoryNone;
  doNotHideItem.accessibilityTraits |= UIAccessibilityTraitButton;
  doNotHideItem.accessibilityIdentifier = kSettingsIncognitoLockDoNotHideCellId;
  [model addItem:doNotHideItem toSectionWithIdentifier:kLockStates];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.presentationDelegate incognitoLockViewControllerDidRemove:self];
  [super viewDidDisappear:animated];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* selectedItem = [self.tableViewModel itemAtIndexPath:indexPath];
  [_mutator updateIncognitoLockState:StateWithItemIdentifier(
                                         static_cast<ItemIdentifier>(
                                             selectedItem.type))];
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - IncognitoLockConsumer

- (void)setIncognitoLockState:(IncognitoLockState)state {
  _selectedState = state;
  [self updateSelectionCheckmark];
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

#pragma mark - Private

- (void)updateSelectionCheckmark {
  NSMutableArray<NSIndexPath*>* indexPaths = [[NSMutableArray alloc] init];
  for (TableViewItem* item in [self.tableViewModel
           itemsInSectionWithIdentifier:SectionIdentifier::kLockStates]) {
    if (item.type == ItemIdentifier::kHeader) {
      continue;
    }
    IncognitoLockState lockState =
        StateWithItemIdentifier(static_cast<ItemIdentifier>(item.type));

    item.accessoryType = lockState == _selectedState
                             ? UITableViewCellAccessoryCheckmark
                             : UITableViewCellAccessoryNone;
    [indexPaths addObject:[self.tableViewModel indexPathForItem:item]];
  }
  [self.tableView reconfigureRowsAtIndexPaths:indexPaths];
}

@end
