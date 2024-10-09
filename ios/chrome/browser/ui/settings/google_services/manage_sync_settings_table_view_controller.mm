// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/authentication/cells/central_account_view.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_service_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller_model_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Table view customized header heights.
CGFloat kAdvancedSettingsSectionHeaderHeightPointSize = 26.;
CGFloat kSignOutSectionHeaderHeightPointSize = 26.;
CGFloat kDefaultSectionHeaderHeightPointSize = 10.;

// Table view customized footer heights.
CGFloat kDefaultSectionFooterHeightPointSize = 10.;

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

#pragma mark - Private

- (void)switchAction:(UISwitch*)sender {
  TableViewModel* model = self.tableViewModel;
  NSIndexPath* indexPath = [model indexPathForItemType:sender.tag];
  DCHECK(indexPath);
  SyncSwitchItem* syncSwitchItem = base::apple::ObjCCastStrict<SyncSwitchItem>(
      [model itemAtIndexPath:indexPath]);
  DCHECK(syncSwitchItem);
  [self.serviceDelegate toggleSwitchItem:syncSwitchItem withValue:sender.isOn];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if ([cell isKindOfClass:[TableViewSwitchCell class]]) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchAction:)
                    forControlEvents:UIControlEventValueChanged];
    ListItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    switchCell.switchView.tag = item.type;
  } else if ([cell isKindOfClass:[TableViewInfoButtonCell class]]) {
    TableViewInfoButtonCell* managedCell =
        base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
    managedCell.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [managedCell.trailingButton addTarget:self
                                   action:@selector(didTapManagedUIInfoButton:)
                         forControlEvents:UIControlEventTouchUpInside];
  } else if ([cell isKindOfClass:[SettingsImageDetailTextCell class]]) {
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
  }
  return cell;
}

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
                            managementState:(ManagementState)managementState {
  // Put a small non-empty frame to avoid layout constraint error during
  // initialization. The actual frame size is changed by the CentralAccountView.
  CentralAccountView* identityAccountItem =
      [[CentralAccountView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                    avatarImage:avatarImage
                                           name:name
                                          email:email
                                managementState:std::move(managementState)
                                useLargeMargins:YES];
  self.tableView.tableHeaderView = identityAccountItem;
  [self.tableView reloadData];
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
  if (self.isAccountStateSignedIn) {
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
  }
  return ChromeTableViewHeightForHeaderInSection(section);
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if (self.isAccountStateSignedIn) {
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSectionIndex:section];
    switch (sectionIdentifier) {
      case SyncDataTypeSectionIdentifier:
      case ManageAndSignOutSectionIdentifier:
        return UITableViewAutomaticDimension;
      case AdvancedSettingsSectionIdentifier:
      case SyncErrorsSectionIdentifier:
        break;
    }
  }
  return kDefaultSectionFooterHeightPointSize;
}

#pragma mark - Sync helpers

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc]
          initWithMessage:self.isAccountStateSignedIn
                              ? l10n_util::GetNSString(
                                    IDS_IOS_ENTERPRISE_MANAGED_SAVE_IN_ACCOUNT)
                              : l10n_util::GetNSString(
                                    IDS_IOS_ENTERPRISE_MANAGED_SYNC)
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
