// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_ai_base_item_type.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_ai_base_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
enum SectionIdentifier {
  SectionIdentifierDriversLicenses = kSectionIdentifierEnumZero,
  SectionIdentifierNationalIdCards,
  SectionIdentifierPassports,
};

}  // namespace

@interface IdentityDocsTableViewController ()
@end

// View controller implementation for Identity Docs.
@implementation IdentityDocsTableViewController {
  NSArray<TableViewItem*>* _driversLicenses;
  NSArray<TableViewItem*>* _nationalIdCards;
  NSArray<TableViewItem*>* _passports;
  BOOL _settingsAreDismissed;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_AUTOFILL_IDENTITY_DOCS_TITLE);
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate identityDocsTableViewControllerDidRemove:self];
  }
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  if (_driversLicenses.count > 0) {
    [model addSectionWithIdentifier:SectionIdentifierDriversLicenses];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text =
        l10n_util::GetNSString(IDS_AUTOFILL_AI_DRIVERS_LICENSES_TITLE);
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierDriversLicenses];
    for (TableViewItem* item in _driversLicenses) {
      [model addItem:item
          toSectionWithIdentifier:SectionIdentifierDriversLicenses];
    }
  }

  if (_nationalIdCards.count > 0) {
    [model addSectionWithIdentifier:SectionIdentifierNationalIdCards];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text = l10n_util::GetNSString(IDS_AUTOFILL_AI_NATIONAL_IDS_TITLE);
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierNationalIdCards];
    for (TableViewItem* item in _nationalIdCards) {
      [model addItem:item
          toSectionWithIdentifier:SectionIdentifierNationalIdCards];
    }
  }

  if (_passports.count > 0) {
    [model addSectionWithIdentifier:SectionIdentifierPassports];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text = l10n_util::GetNSString(IDS_AUTOFILL_AI_PASSPORTS_TITLE);
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierPassports];
    for (TableViewItem* item in _passports) {
      [model addItem:item toSectionWithIdentifier:SectionIdentifierPassports];
    }
  }
}

#pragma mark - IdentityDocsConsumer

- (void)
    setIdentityDocsWithDriversLicenses:(NSArray<TableViewItem*>*)driversLicenses
                       nationalIdCards:(NSArray<TableViewItem*>*)nationalIdCards
                             passports:(NSArray<TableViewItem*>*)passports {
  _driversLicenses = driversLicenses;
  _nationalIdCards = nationalIdCards;
  _passports = passports;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  [self.mutator didSelectEntityItem:item];
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
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
