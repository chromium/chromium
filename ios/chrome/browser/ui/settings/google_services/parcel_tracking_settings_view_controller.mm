// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_opt_in_status.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_model_delegate.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

enum SectionIdentifier {
  kSectionIdentifierTrackingOptions = kSectionIdentifierEnumZero,
};

enum ItemType {
  kAutoTrackParcelItem = kItemTypeEnumZero,
  kAskEveryTimeParcelItem,
  kNeverAutoDetectParcelItem,
  kFooterItem,
};

// Converts an ItemType, to a corresponding IOSParcelTrackingOptInStatus.
IOSParcelTrackingOptInStatus OptInStatusForItemType(ItemType item_type) {
  switch (item_type) {
    case kAutoTrackParcelItem: {
      return IOSParcelTrackingOptInStatus::kAlwaysTrack;
    }
    case kAskEveryTimeParcelItem: {
      return IOSParcelTrackingOptInStatus::kAskToTrack;
    }
    case kNeverAutoDetectParcelItem: {
      return IOSParcelTrackingOptInStatus::kNeverTrack;
    }
    case kFooterItem:
      NOTREACHED_IN_MIGRATION();
      return IOSParcelTrackingOptInStatus::kNeverTrack;
  }
}

}  // namespace.

@implementation ParcelTrackingSettingsViewController {
  IOSParcelTrackingOptInStatus _latestOptInState;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE);
  [self loadModel];
}

#pragma mark - TableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:kSectionIdentifierTrackingOptions];

  TableViewDetailTextItem* alwaysTrackItem =
      [[TableViewDetailTextItem alloc] initWithType:kAutoTrackParcelItem];
  alwaysTrackItem.text = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTO_TRACK_PACKAGES_ALL);
  alwaysTrackItem.accessibilityTraits = UIAccessibilityTraitButton;
  [model addItem:alwaysTrackItem
      toSectionWithIdentifier:kSectionIdentifierTrackingOptions];

  TableViewDetailTextItem* askEveryTimeItem =
      [[TableViewDetailTextItem alloc] initWithType:kAskEveryTimeParcelItem];
  askEveryTimeItem.text =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_TERTIARY_ACTION);
  askEveryTimeItem.accessibilityTraits = UIAccessibilityTraitButton;
  [model addItem:askEveryTimeItem
      toSectionWithIdentifier:kSectionIdentifierTrackingOptions];

  TableViewDetailTextItem* neverTrackItem =
      [[TableViewDetailTextItem alloc] initWithType:kNeverAutoDetectParcelItem];
  neverTrackItem.text = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTO_TRACK_PACKAGES_NEVER);
  neverTrackItem.accessibilityTraits = UIAccessibilityTraitButton;
  [model addItem:neverTrackItem
      toSectionWithIdentifier:kSectionIdentifierTrackingOptions];

  TableViewTextHeaderFooterItem* footerItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:kFooterItem];
  footerItem.subtitle = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTO_TRACK_PACKAGES_FOOTER);
  [model setFooter:footerItem
      forSectionWithIdentifier:kSectionIdentifierTrackingOptions];

  [self updateCheckedState:_latestOptInState];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewDetailTextItem* item = static_cast<TableViewDetailTextItem*>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  [self.modelDelegate
      tableViewDidSelectStatus:OptInStatusForItemType(
                                   static_cast<ItemType>(item.type))];
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Private

// Updates the checked state of the cells to match the preferences.
- (void)updateCheckedState:(IOSParcelTrackingOptInStatus)newState {
  _latestOptInState = newState;

  if (!self.viewLoaded) {
    return;
  }

  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (TableViewDetailTextItem* item in [self.tableViewModel
           itemsInSectionWithIdentifier:kSectionIdentifierTrackingOptions]) {
    IOSParcelTrackingOptInStatus itemSetting =
        OptInStatusForItemType(static_cast<ItemType>(item.type));

    UITableViewCellAccessoryType desiredType =
        itemSetting == newState ? UITableViewCellAccessoryCheckmark
                                : UITableViewCellAccessoryNone;

    // If the status is not explicitly set (default), then "Ask To Track" should
    // be selected. kStatusNotSet and kAskToTrack have the same behavior and are
    // only differentiated for metrics.
    if (newState == IOSParcelTrackingOptInStatus::kStatusNotSet &&
        itemSetting == IOSParcelTrackingOptInStatus::kAskToTrack) {
      desiredType = UITableViewCellAccessoryCheckmark;
    }

    if (item.accessoryType != desiredType) {
      item.accessoryType = desiredType;
      [modifiedItems addObject:item];
    }
  }

  [self reconfigureCellsForItems:modifiedItems];
}

@end
