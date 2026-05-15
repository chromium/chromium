// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_table_view_controller.h"

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
  SectionIdentifierFlightReservations = kSectionIdentifierEnumZero,
  SectionIdentifierKnownTravelerNumbers,
  SectionIdentifierRedressNumbers,
  SectionIdentifierVehicles,
};

}  // namespace

@interface TravelInfoTableViewController ()
@end

// View controller implementation for Travel Info.
@implementation TravelInfoTableViewController {
  NSArray<TableViewItem*>* _flightReservations;
  NSArray<TableViewItem*>* _knownTravelerNumbers;
  NSArray<TableViewItem*>* _redressNumbers;
  NSArray<TableViewItem*>* _vehicles;
  BOOL _settingsAreDismissed;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_AUTOFILL_TRAVEL_TITLE);
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate travelInfoTableViewControllerDidRemove:self];
  }
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  if (_flightReservations.count > 0) {
    [model addSectionWithIdentifier:SectionIdentifierFlightReservations];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text =
        l10n_util::GetNSString(IDS_AUTOFILL_AI_FLIGHT_RESERVATIONS_TITLE);
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierFlightReservations];
    for (TableViewItem* item in _flightReservations) {
      [model addItem:item
          toSectionWithIdentifier:SectionIdentifierFlightReservations];
    }
  }

  if (_knownTravelerNumbers.count > 0) {
    [model addSectionWithIdentifier:SectionIdentifierKnownTravelerNumbers];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text = l10n_util::GetNSString(
        IDS_AUTOFILL_AI_KNOWN_TRAVELER_NUMBER_ENTITY_NAME);
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierKnownTravelerNumbers];
    for (TableViewItem* item in _knownTravelerNumbers) {
      [model addItem:item
          toSectionWithIdentifier:SectionIdentifierKnownTravelerNumbers];
    }
  }

  if (_redressNumbers.count > 0) {
    [model addSectionWithIdentifier:SectionIdentifierRedressNumbers];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text =
        l10n_util::GetNSString(IDS_AUTOFILL_AI_REDRESS_NUMBER_ENTITY_NAME);
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierRedressNumbers];
    for (TableViewItem* item in _redressNumbers) {
      [model addItem:item
          toSectionWithIdentifier:SectionIdentifierRedressNumbers];
    }
  }

  if (_vehicles.count > 0) {
    [model addSectionWithIdentifier:SectionIdentifierVehicles];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text = l10n_util::GetNSString(IDS_AUTOFILL_AI_VEHICLES_TITLE);
    [model setHeader:header forSectionWithIdentifier:SectionIdentifierVehicles];
    for (TableViewItem* item in _vehicles) {
      [model addItem:item toSectionWithIdentifier:SectionIdentifierVehicles];
    }
  }
}

#pragma mark - TravelInfoConsumer

- (void)setTravelInfoWithFlightReservations:
            (NSArray<TableViewItem*>*)flightReservations
                       knownTravelerNumbers:
                           (NSArray<TableViewItem*>*)knownTravelerNumbers
                             redressNumbers:
                                 (NSArray<TableViewItem*>*)redressNumbers
                                   vehicles:(NSArray<TableViewItem*>*)vehicles {
  _flightReservations = flightReservations;
  _knownTravelerNumbers = knownTravelerNumbers;
  _redressNumbers = redressNumbers;
  _vehicles = vehicles;
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
