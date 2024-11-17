// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller.h"

#import <LocalAuthentication/LocalAuthentication.h>

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/elements/info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_mutator.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

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
  kHideWithReauthDisabled,
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
    case ItemIdentifier::kHideWithReauthDisabled:
    case ItemIdentifier::kHeader:
      NOTREACHED();
  }
}

}  // namespace

@interface IncognitoLockViewController () <PopoverLabelViewControllerDelegate>
@end

@implementation IncognitoLockViewController {
  IncognitoLockState _selectedState;
  id<ReauthenticationProtocol> _reauthModule;
}

- (instancetype)initWithReauthModule:
    (id<ReauthenticationProtocol>)reauthModule {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _reauthModule = reauthModule;
  }
  return self;
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

  TableViewItem* hideWithReauthItem = [_reauthModule canAttemptReauth]
                                          ? self.hideWithReauthItem
                                          : self.hideWithReauthDisabledItem;
  [model addItem:hideWithReauthItem toSectionWithIdentifier:kLockStates];

  TableViewDetailTextItem* hideWithSoftLockItem =
      [[TableViewDetailTextItem alloc] initWithType:kHideWithSoftLock];
  hideWithSoftLockItem.text =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_INCOGNITO_LOCK_HIDE_WITH_SOFT_LOCK_TITLE,
          kIOSSoftLockBackgroundThreshold.Get().InMinutes()));
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
  ItemIdentifier itemIdentifier =
      static_cast<ItemIdentifier>(selectedItem.type);

  if (itemIdentifier == kHideWithReauthDisabled ||
      StateWithItemIdentifier(itemIdentifier) == _selectedState) {
    // Do nothing on disabled reauth item clicks or item clicks where the state
    // is the same as the currently selected item.
  } else if (itemIdentifier == kHideWithReauth ||
             _selectedState == IncognitoLockState::kReauth) {
    // Require authentication on transitions to/from the reauth state.
    id<IncognitoLockMutator> mutator = _mutator;
    [_reauthModule
        attemptReauthWithLocalizedReason:
            l10n_util::GetNSString(
                IDS_IOS_INCOGNITO_REAUTH_SET_UP_SYSTEM_DIALOG_REASON)
                    canReusePreviousAuth:false
                                 handler:^(ReauthenticationResult result) {
                                   if (result ==
                                       ReauthenticationResult::kSuccess) {
                                     // Only update the state if the
                                     // authentication is successful.
                                     [mutator updateIncognitoLockState:
                                                  StateWithItemIdentifier(
                                                      itemIdentifier)];
                                   }
                                 }];
  } else {
    [_mutator updateIncognitoLockState:StateWithItemIdentifier(itemIdentifier)];
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemIdentifier itemIdentifier = static_cast<ItemIdentifier>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (itemIdentifier == kHideWithReauthDisabled) {
    TableViewInfoButtonCell* managedCell =
        base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
    [managedCell.trailingButton
               addTarget:self
                  action:@selector(didTapHideWithReauthDisabledInfoButton:)
        forControlEvents:UIControlEventTouchUpInside];
  }
  return cell;
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

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [super view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

#pragma mark - Private

// Updates the checkmark visibility of each item, based on the current Incognito
// lock state.
- (void)updateSelectionCheckmark {
  NSMutableArray<NSIndexPath*>* indexPaths = [[NSMutableArray alloc] init];
  for (TableViewItem* item in [self.tableViewModel
           itemsInSectionWithIdentifier:SectionIdentifier::kLockStates]) {
    if (item.type == ItemIdentifier::kHeader ||
        item.type == kHideWithReauthDisabled) {
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

// Item corresponding to the Incognito reauth setting, displayed when an
// authentication method is available.
- (TableViewTextItem*)hideWithReauthItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:kHideWithReauth];
  item.text = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_LOCK_HIDE_WITH_REAUTH,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  item.accessoryType = _selectedState == IncognitoLockState::kReauth
                           ? UITableViewCellAccessoryCheckmark
                           : UITableViewCellAccessoryNone;
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessibilityIdentifier = kSettingsIncognitoLockHideWithReauthCellId;
  return item;
}

// Item corresponding to the Incognito reauth setting, displayed when no
// authentication methods are available.
- (TableViewInfoButtonItem*)hideWithReauthDisabledItem {
  TableViewInfoButtonItem* item =
      [[TableViewInfoButtonItem alloc] initWithType:kHideWithReauthDisabled];
  item.text = l10n_util::GetNSString(IDS_IOS_INCOGNITO_REAUTH_SETTING_NAME);
  item.iconTintColor = [UIColor colorNamed:kGrey300Color];
  item.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return item;
}

// Callback invoked when the info button of the Incognito reauth disabled
// setting is tapped.
- (void)didTapHideWithReauthDisabledInfoButton:(UIButton*)buttonView {
  InfoPopoverViewController* popover = [[InfoPopoverViewController alloc]
      initWithMessage:l10n_util::GetNSString(
                          IDS_IOS_INCOGNITO_REAUTH_SET_UP_PASSCODE_HINT)];

  [self showInfoPopover:popover forInfoButton:buttonView];
}

// Shows a contextual bubble explaining that the tapped setting is managed and
// includes a link to the chrome://management page.
- (void)showInfoPopover:(PopoverLabelViewController*)popover
          forInfoButton:(UIButton*)buttonView {
  popover.delegate = self;

  // Disable the button when showing the bubble.
  // The button will be enabled when closing the bubble in
  // (void)popoverPresentationControllerDidDismissPopover: of
  // EnterpriseInfoPopoverViewController.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  popover.popoverPresentationController.sourceView = buttonView;
  popover.popoverPresentationController.sourceRect = buttonView.bounds;
  popover.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [self presentViewController:popover animated:YES completion:nil];
}

@end
