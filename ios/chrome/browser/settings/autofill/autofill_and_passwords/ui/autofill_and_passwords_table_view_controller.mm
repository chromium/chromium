// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_table_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/utils/autofill_and_passwords_item_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AutofillAndPasswordsTableViewController ()
@end

@implementation AutofillAndPasswordsTableViewController {
  // State variables.
  BOOL _passwordsEnabled;
  BOOL _autofillCreditCardEnabled;
  BOOL _autofillProfileEnabled;
  BOOL _identityDocsEnabled;
  BOOL _travelInfoEnabled;
  BOOL _shouldShowAutofillAIFeatures;

  // Updatable Items.
  TableViewDetailIconItem* _passwordsDetailItem;
  TableViewDetailIconItem* _autofillCreditCardDetailItem;
  TableViewDetailIconItem* _autofillProfileDetailItem;
  TableViewDetailIconItem* _identityDocsDetailItem;
  TableViewDetailIconItem* _travelInfoDetailItem;

  BOOL _settingsAreDismissed;
}

- (instancetype)initWithStyle:(UITableViewStyle)style {
  self = [super initWithStyle:style];
  if (self) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_AND_PASSWORDS);
  }
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate autofillAndPasswordsTableViewControllerDidRemove:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel<TableViewItem*>* model = self.tableViewModel;
  [model addSectionWithIdentifier:SettingsSectionIdentifierBasics];

  _passwordsDetailItem = PasswordsItem(_passwordsEnabled);
  [model addItem:_passwordsDetailItem
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];

  _autofillCreditCardDetailItem =
      AutofillCreditCardItem(_autofillCreditCardEnabled);
  [model addItem:_autofillCreditCardDetailItem
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];

  _autofillProfileDetailItem = AutofillProfileItem(_autofillProfileEnabled);
  [model addItem:_autofillProfileDetailItem
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];

  if (_shouldShowAutofillAIFeatures) {
    _identityDocsDetailItem = IdentityDocsItem(_identityDocsEnabled);
    [model addItem:_identityDocsDetailItem
        toSectionWithIdentifier:SettingsSectionIdentifierBasics];

    _travelInfoDetailItem = TravelInfoItem(_travelInfoEnabled);
    [model addItem:_travelInfoDetailItem
        toSectionWithIdentifier:SettingsSectionIdentifierBasics];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case SettingsItemTypePasswords:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectPasswords:self];
      break;
    case SettingsItemTypeAutofillCreditCard:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectAutofillCreditCard:
              self];
      break;
    case SettingsItemTypeAutofillProfile:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectAutofillProfile:self];
      break;
    case SettingsItemTypeIdentityDocs:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectIdentityDocs:self];
      break;
    case SettingsItemTypeTravelInfo:
      [self.delegate
          autofillAndPasswordsTableViewControllerDidSelectTravelInfo:self];
      break;
    default:
      break;
  }
}

#pragma mark - AutofillAndPasswordsConsumer

- (void)setPasswordsEnabled:(BOOL)enabled {
  if (_passwordsEnabled == enabled) {
    return;
  }
  _passwordsEnabled = enabled;

  if (_passwordsDetailItem) {
    _passwordsDetailItem.detailText = PasswordsItemDetailText(enabled);
    [self reconfigureCellsForItems:@[ _passwordsDetailItem ]];
  }
}

- (void)setAutofillCreditCardEnabled:(BOOL)enabled {
  if (_autofillCreditCardEnabled == enabled) {
    return;
  }
  _autofillCreditCardEnabled = enabled;

  if (_autofillCreditCardDetailItem) {
    _autofillCreditCardDetailItem.detailText =
        AutofillCreditCardItemDetailText(enabled);
    [self reconfigureCellsForItems:@[ _autofillCreditCardDetailItem ]];
  }
}

- (void)setAutofillProfileEnabled:(BOOL)enabled {
  if (_autofillProfileEnabled == enabled) {
    return;
  }
  _autofillProfileEnabled = enabled;

  if (_autofillProfileDetailItem) {
    _autofillProfileDetailItem.detailText =
        AutofillProfileItemDetailText(enabled);
    [self reconfigureCellsForItems:@[ _autofillProfileDetailItem ]];
  }
}

- (void)setIdentityDocsEnabled:(BOOL)enabled {
  if (_identityDocsEnabled == enabled) {
    return;
  }
  _identityDocsEnabled = enabled;

  if (_identityDocsDetailItem) {
    _identityDocsDetailItem.detailText = IdentityDocsItemDetailText(enabled);
    [self reconfigureCellsForItems:@[ _identityDocsDetailItem ]];
  }
}

- (void)setTravelInfoEnabled:(BOOL)enabled {
  if (_travelInfoEnabled == enabled) {
    return;
  }
  _travelInfoEnabled = enabled;

  if (_travelInfoDetailItem) {
    _travelInfoDetailItem.detailText = TravelInfoItemDetailText(enabled);
    [self reconfigureCellsForItems:@[ _travelInfoDetailItem ]];
  }
}

- (void)setShouldShowAutofillAIFeatures:(BOOL)shouldShow {
  _shouldShowAutofillAIFeatures = shouldShow;
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/500341282): Add missing metric.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/500341282): Add missing metric.
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  _settingsAreDismissed = YES;
}

@end
