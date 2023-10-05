// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_mediator.h"

#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_model_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

enum SectionIdentifier {
  SectionIdentifierTrackingOptions = kSectionIdentifierEnumZero,
};

enum ItemType {
  AutoTrackParcelItem = kItemTypeEnumZero,
  AskEveryTimeParcelItem,
  NeverAutoDetectParcelItem,
  FooterItem,
};

// Converts an ItemType, to a corresponding IOSParcelTrackingOptInStatus.
IOSParcelTrackingOptInStatus OptInStatusForItemType(ItemType item_type) {
  switch (item_type) {
    case AutoTrackParcelItem: {
      return IOSParcelTrackingOptInStatus::kAlwaysTrack;
    }
    case AskEveryTimeParcelItem: {
      return IOSParcelTrackingOptInStatus::kAskToTrack;
    }
    case NeverAutoDetectParcelItem: {
      return IOSParcelTrackingOptInStatus::kNeverTrack;
    }
    case FooterItem:
      NOTREACHED();
      return IOSParcelTrackingOptInStatus::kNeverTrack;
  }
}

}  // namespace.

@interface ParcelTrackingSettingsMediator () <PrefObserverDelegate>
@end

@implementation ParcelTrackingSettingsMediator {
  IntegerPrefMember _optInStatus;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

- (instancetype)initWithPrefs:(PrefService*)prefs {
  self = [super init];
  if (self) {
    _optInStatus.Init(prefs::kIosParcelTrackingOptInStatus, prefs);

    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(prefs);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosParcelTrackingOptInStatus, _prefChangeRegistrar.get());
  }
  return self;
}

#pragma mark - ParcelTrackingSettingsModelDelegate

- (void)loadModel {
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierTrackingOptions];

  TableViewDetailTextItem* alwaysTrackItem =
      [[TableViewDetailTextItem alloc] initWithType:AutoTrackParcelItem];
  alwaysTrackItem.text = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTO_TRACK_PACKAGES_ALL);
  alwaysTrackItem.accessibilityTraits = UIAccessibilityTraitButton;
  [model addItem:alwaysTrackItem
      toSectionWithIdentifier:SectionIdentifierTrackingOptions];

  TableViewDetailTextItem* askEveryTimeItem =
      [[TableViewDetailTextItem alloc] initWithType:AskEveryTimeParcelItem];
  askEveryTimeItem.text =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_TERTIARY_ACTION);
  askEveryTimeItem.accessibilityTraits = UIAccessibilityTraitButton;
  [model addItem:askEveryTimeItem
      toSectionWithIdentifier:SectionIdentifierTrackingOptions];

  TableViewDetailTextItem* neverTrackItem =
      [[TableViewDetailTextItem alloc] initWithType:NeverAutoDetectParcelItem];
  neverTrackItem.text = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTO_TRACK_PACKAGES_NEVER);
  neverTrackItem.accessibilityTraits = UIAccessibilityTraitButton;
  [model addItem:neverTrackItem
      toSectionWithIdentifier:SectionIdentifierTrackingOptions];

  TableViewTextHeaderFooterItem* footerItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:FooterItem];
  footerItem.subtitle = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTO_TRACK_PACKAGES_FOOTER);
  [model setFooter:footerItem
      forSectionWithIdentifier:SectionIdentifierTrackingOptions];

  [self updateCheckedState];
}

- (void)tableViewDidSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  IOSParcelTrackingOptInStatus chosenSetting =
      OptInStatusForItemType(static_cast<ItemType>(
          [self.consumer.tableViewModel itemTypeForIndexPath:indexPath]));
  [self updateSetting:chosenSetting];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kIosParcelTrackingOptInStatus) {
    [self updateCheckedState];
  }
}

#pragma mark - Helpers

// Updates the pref value with `newSetting`.
- (void)updateSetting:(IOSParcelTrackingOptInStatus)newSetting {
  _optInStatus.SetValue(static_cast<int>(newSetting));
}

// Updates the checked state of the cells to match the preferences.
- (void)updateCheckedState {
  IOSParcelTrackingOptInStatus status =
      static_cast<IOSParcelTrackingOptInStatus>(_optInStatus.GetValue());

  TableViewModel<TableViewItem*>* model = self.consumer.tableViewModel;

  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (TableViewDetailTextItem* item in
       [model itemsInSectionWithIdentifier:SectionIdentifierTrackingOptions]) {
    IOSParcelTrackingOptInStatus itemSetting =
        OptInStatusForItemType(static_cast<ItemType>(item.type));

    UITableViewCellAccessoryType desiredType =
        itemSetting == status ? UITableViewCellAccessoryCheckmark
                              : UITableViewCellAccessoryNone;
    if (item.accessoryType != desiredType) {
      item.accessoryType = desiredType;
      [modifiedItems addObject:item];
    }
  }

  [self.consumer reconfigureCellsForItems:modifiedItems];
}

@end
