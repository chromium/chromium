// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_cvc_storage_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSaveSecurityCodesSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierDeleteSavedSecurityCodes
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSaveSecurityCodesSwitch = kItemTypeEnumZero,
  ItemTypeSaveSecurityCodesSwitchSubtitle,
  ItemTypeDeleteSavedSecurityCodesButton,
  ItemTypeDeleteSavedSecurityCodesButtonSubtitle,
};

}  // namespace

@implementation AutofillCvcStorageViewController {
  // Coordinator for displaying an action sheet.
  UIAlertController* _alertController;
}

@synthesize cvcStorageSwitchIsOn = _cvcStorageSwitchIsOn;
@synthesize hasSavedCvcs = _hasSavedCvcs;

- (void)setCvcStorageSwitchIsOn:(BOOL)cvcStorageSwitchIsOn {
  if (_cvcStorageSwitchIsOn == cvcStorageSwitchIsOn) {
    return;
  }
  _cvcStorageSwitchIsOn = cvcStorageSwitchIsOn;
  if (self.isViewLoaded) {
    [self loadModel];
    [self.tableView reloadData];
  }
}

- (void)setHasSavedCvcs:(BOOL)hasSavedCvcs {
  if (_hasSavedCvcs == hasSavedCvcs) {
    return;
  }
  _hasSavedCvcs = hasSavedCvcs;
  if (self.isViewLoaded) {
    [self loadModel];
    [self.tableView reloadData];
  }
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(
        IDS_AUTOFILL_SETTINGS_PAGE_ENABLE_CVC_STORAGE_LABEL);
    self.shouldDisableDoneButtonOnEdit = YES;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kAutofillSecurityCvcsTableViewId;
  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSaveSecurityCodesSwitch];
  [model addItem:[self securityCodesSwitchItem]
      toSectionWithIdentifier:SectionIdentifierSaveSecurityCodesSwitch];

  [model setFooter:[self securityCodesSwitchFooter]
      forSectionWithIdentifier:SectionIdentifierSaveSecurityCodesSwitch];

  if (self.hasSavedCvcs) {
    [model addSectionWithIdentifier:SectionIdentifierDeleteSavedSecurityCodes];
    [model addItem:[self deleteSecurityCvcsItem]
        toSectionWithIdentifier:SectionIdentifierDeleteSavedSecurityCodes];
    [model setFooter:[self deleteSecurityCvcsFooter]
        forSectionWithIdentifier:SectionIdentifierDeleteSavedSecurityCodes];
  }
}

#pragma mark - LoadModel Helpers

- (TableViewItem*)securityCodesSwitchItem {
  TableViewSwitchItem* switchItem = [[TableViewSwitchItem alloc]
      initWithType:ItemTypeSaveSecurityCodesSwitch];
  switchItem.text = l10n_util::GetNSString(
      IDS_AUTOFILL_SETTINGS_PAGE_ENABLE_CVC_STORAGE_LABEL);
  switchItem.target = self;
  switchItem.selector = @selector(cvcStorageSwitchChanged:);
  switchItem.on = self.cvcStorageSwitchIsOn;
  switchItem.accessibilityIdentifier = kAutofillSaveSecurityCodesSwitchViewId;
  return switchItem;
}

- (TableViewHeaderFooterItem*)securityCodesSwitchFooter {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeSaveSecurityCodesSwitchSubtitle];
  footer.text = l10n_util::GetNSString(
      IDS_AUTOFILL_SETTINGS_PAGE_ENABLE_CVC_STORAGE_SUBLABEL);
  return footer;
}

- (TableViewItem*)deleteSecurityCvcsItem {
  TableViewTextItem* deleteSecurityCvcItem = [[TableViewTextItem alloc]
      initWithType:ItemTypeDeleteSavedSecurityCodesButton];
  deleteSecurityCvcItem.text =
      l10n_util::GetNSString(IDS_AUTOFILL_SETTINGS_PAGE_BULK_REMOVE_CVC_LABEL);
  deleteSecurityCvcItem.accessibilityIdentifier =
      kAutofillDeleteSecurityCodesButtonId;
  deleteSecurityCvcItem.accessibilityTraits = UIAccessibilityTraitButton;
  deleteSecurityCvcItem.textColor = [UIColor colorNamed:kRedColor];
  return deleteSecurityCvcItem;
}

- (TableViewHeaderFooterItem*)deleteSecurityCvcsFooter {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeDeleteSavedSecurityCodesButtonSubtitle];
  footer.text = l10n_util::GetNSString(
      IDS_AUTOFILL_SETTINGS_PAGE_DELETE_CVC_STORAGE_LABEL);
  return footer;
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldShowEditButton {
  return NO;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  TableViewModel* model = self.tableViewModel;
  NSInteger type = [model itemTypeForIndexPath:indexPath];

  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  if (type == ItemTypeDeleteSavedSecurityCodesButton) {
    base::RecordAction(
        base::UserMetricsAction("BulkCvcDeletionHyperlinkClicked"));
    [self showDeleteConfirmationForIndexPath:indexPath];
  }
}

#pragma mark - Switch Callbacks

- (void)cvcStorageSwitchChanged:(UISwitch*)switchView {
  [self.delegate viewController:self
      didChangeCvcStorageSwitchTo:[switchView isOn]];
}

#pragma mark - Private

- (void)showDeleteConfirmationForIndexPath:(NSIndexPath*)indexPath {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(
              IDS_AUTOFILL_SETTINGS_PAGE_BULK_REMOVE_CVC_TITLE)
                       message:
                           l10n_util::GetNSString(
                               IDS_AUTOFILL_SETTINGS_PAGE_BULK_REMOVE_CVC_DESCRIPTION)
                preferredStyle:UIAlertControllerStyleActionSheet];

  __weak __typeof(self) weakSelf = self;
  UIAlertAction* deleteAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_SAVED_SECURITY_CODE)
                style:UIAlertActionStyleDestructive
              handler:^(UIAlertAction* action) {
                base::RecordAction(base::UserMetricsAction(
                    "BulkCvcDeletionConfirmationDialogAccepted"));
                [weakSelf.delegate
                    deleteAllSavedCvcsForViewController:weakSelf];
                [weakSelf userDismissedAlert];
              }];

  UIAlertAction* cancelAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_DELETE_SAVED_SECURITY_CODE_CANCEL)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                base::RecordAction(base::UserMetricsAction(
                    "BulkCvcDeletionConfirmationDialogCancelled"));
                [weakSelf userDismissedAlert];
              }];

  [alertController addAction:deleteAction];
  [alertController addAction:cancelAction];

  // Set popover source for iPad.
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  alertController.popoverPresentationController.sourceView = cell;
  alertController.popoverPresentationController.sourceRect = cell.bounds;

  _alertController = alertController;
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)userDismissedAlert {
  _alertController = nil;
}

@end
