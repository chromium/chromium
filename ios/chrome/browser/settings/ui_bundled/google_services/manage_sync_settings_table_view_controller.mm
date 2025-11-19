// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/central_account_view.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/sync_switch_item.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_service_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_table_view_controller_model_delegate.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Table view customized header heights.
constexpr CGFloat kAdvancedSettingsSectionHeaderHeightPointSize = 26.;
constexpr CGFloat kSignOutSectionHeaderHeightPointSize = 26.;
constexpr CGFloat kDefaultSectionHeaderHeightPointSize = 10.;

// Table view customized footer heights.
constexpr CGFloat kDefaultSectionFooterHeightPointSize = 10.;

}  // namespace

@interface ManageSyncSettingsTableViewController () <
    PopoverLabelViewControllerDelegate>
@end

@implementation ManageSyncSettingsTableViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      kManageSyncTableViewAccessibilityIdentifier;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self reloadData];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        manageSyncSettingsTableViewControllerWasRemoved:self];
  }
}

#pragma mark - UITableViewDataSource

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];

  if (![self.tableViewModel footerForSectionIndex:section]) {
    // Don't set up the footer view when there isn't a footer in the model.
    return view;
  }

  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (sectionIdentifier == ManageAndSignOutSectionIdentifier) {
    TableViewLinkHeaderFooterView* linkView =
        base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
    linkView.delegate = self;
  }

  return view;
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  [self.modelDelegate manageSyncSettingsTableViewControllerLoadModel:self];
}

#pragma mark - ManageSyncSettingsConsumer

- (void)insertSections:(NSIndexSet*)sections rowAnimation:(BOOL)rowAnimation {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  if (rowAnimation) {
    [self.tableView insertSections:sections
                  withRowAnimation:UITableViewRowAnimationMiddle];
  } else {
    [UIView performWithoutAnimation:^{
      [self.tableView beginUpdates];
      [self.tableView insertSections:sections
                    withRowAnimation:UITableViewRowAnimationNone];
      [self.tableView endUpdates];
    }];
  }
}

- (void)deleteSections:(NSIndexSet*)sections rowAnimation:(BOOL)rowAnimation {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  if (rowAnimation) {
    [self.tableView deleteSections:sections
                  withRowAnimation:UITableViewRowAnimationMiddle];
  } else {
    // To avoid animation glitches related to crbug.com/1469539.
    [UIView performWithoutAnimation:^{
      [self.tableView beginUpdates];
      [self.tableView deleteSections:sections
                    withRowAnimation:UITableViewRowAnimationNone];
      [self.tableView endUpdates];
    }];
  }
}

- (void)reloadItem:(TableViewItem*)item {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  if (!item) {
    // No need to reload if the item doesn't exist. indexPathForItem below
    // should handle nil just fine, but doesn't hurt to early return explicitly.
    return;
  }
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
  if (!indexPath) {
    // No need to reload if the item is not in the model. This would also cause
    // a crash below since NSArrays cannot contain nil.
    // TODO(crbug.com/40073025): Better understand the crash root cause and
    // CHECK instead of no-op.
    return;
  }
  // To avoid animation glitches related to crbug.com/1469539.
  [UIView performWithoutAnimation:^{
    [self.tableView beginUpdates];
    [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                          withRowAnimation:UITableViewRowAnimationNone];
    [self.tableView endUpdates];
  }];
}

- (void)reloadSections:(NSIndexSet*)sections {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.tableView reloadSections:sections
                withRowAnimation:UITableViewRowAnimationNone];
}

- (void)updatePrimaryAccountWithAvatarImage:(UIImage*)avatarImage
                                       name:(NSString*)name
                                      email:(NSString*)email
                      managementDescription:(NSString*)managementDescription {
  CHECK(email);
  CHECK(avatarImage);
  // Put a small non-empty frame to avoid layout constraint error during
  // initialization. The actual frame size is changed by the CentralAccountView.
  CentralAccountView* identityAccountItem =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:avatarImage
                                           name:name
                                          email:email
                          managementDescription:managementDescription
                                useLargeMargins:YES];
  self.tableView.tableHeaderView = identityAccountItem;
  [self.tableView reloadData];
}

- (void)showManagedUIInfoForButton:(UIButton*)button {
  [self didTapManagedUIInfoButton:button];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  CGRect cellRect = [tableView rectForRowAtIndexPath:indexPath];
  [self.serviceDelegate didSelectItem:item cellRect:cellRect];
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSectionIndex:section];
    switch (sectionIdentifier) {
      case SyncDataTypeSectionIdentifier:
        return UITableViewAutomaticDimension;
      case AdvancedSettingsSectionIdentifier:
        return kAdvancedSettingsSectionHeaderHeightPointSize;
      case ManageAndSignOutSectionIdentifier:
        if (![self.tableViewModel hasSectionForSectionIdentifier:
                                      AdvancedSettingsSectionIdentifier]) {
          return kSignOutSectionHeaderHeightPointSize;
        }
        break;
      case SyncErrorsSectionIdentifier:
      case BatchUploadSectionIdentifier:
        return kDefaultSectionHeaderHeightPointSize;
  }
  return ChromeTableViewHeightForHeaderInSection(section);
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSectionIndex:section];
    switch (sectionIdentifier) {
      case SyncDataTypeSectionIdentifier:
        return UITableViewAutomaticDimension;
      case ManageAndSignOutSectionIdentifier: {
        if (AreSeparateProfilesForManagedAccountsEnabled()) {
          break;
        }
        return UITableViewAutomaticDimension;
      }
      case SwitchAccountAndSignOutSectionIdentifier: {
        CHECK(AreSeparateProfilesForManagedAccountsEnabled());
        return UITableViewAutomaticDimension;
      }
      case AdvancedSettingsSectionIdentifier:
      case SyncErrorsSectionIdentifier:
        break;
  }
  return kDefaultSectionFooterHeightPointSize;
}

#pragma mark - Sync helpers

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc]
          initWithMessage:l10n_util::GetNSString(
                              IDS_IOS_ENTERPRISE_MANAGED_SAVE_IN_ACCOUNT)
           enterpriseName:nil];
  [self presentViewController:bubbleViewController animated:YES completion:nil];

  // Disable the button when showing the bubble.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
  bubbleViewController.delegate = self;
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

@end
