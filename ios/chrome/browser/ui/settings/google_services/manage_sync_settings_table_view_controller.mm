// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_service_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_view_controller_model_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  SyncSwitchItem* syncSwitchItem = base::mac::ObjCCastStrict<SyncSwitchItem>(
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
        base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchAction:)
                    forControlEvents:UIControlEventValueChanged];
    ListItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    switchCell.switchView.tag = item.type;
  } else if ([cell isKindOfClass:[TableViewInfoButtonCell class]]) {
    TableViewInfoButtonCell* managedCell =
        base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
    managedCell.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [managedCell.trailingButton addTarget:self
                                   action:@selector(didTapManagedUIInfoButton:)
                         forControlEvents:UIControlEventTouchUpInside];
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

  if (sectionIdentifier == SignOutSectionIdentifier) {
    TableViewLinkHeaderFooterView* linkView =
        base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
    linkView.delegate = self;
  }

  return view;
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  [self.modelDelegate manageSyncSettingsTableViewControllerLoadModel:self];
}

#pragma mark - ManageSyncSettingsConsumer

- (void)insertSections:(NSIndexSet*)sections {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.tableView insertSections:sections
                withRowAnimation:UITableViewRowAnimationNone];
}

- (void)deleteSections:(NSIndexSet*)sections {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.tableView deleteSections:sections
                withRowAnimation:UITableViewRowAnimationNone];
}

- (void)reloadItem:(TableViewItem*)item {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];
}

- (void)reloadSections:(NSIndexSet*)sections {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.tableView reloadSections:sections
                withRowAnimation:UITableViewRowAnimationNone];
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

#pragma mark - Sync helpers

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc]
          initWithMessage:l10n_util::GetNSString(
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
