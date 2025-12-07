// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/bwg_location_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/bwg_settings_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Section identifiers in the BWG Location settings table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLocation = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLocation = kItemTypeEnumZero,
  ItemTypeLocationFooter,
};

// Table identifier.
NSString* const kBWGLocationViewTableIdentifier =
    @"BWGLocationViewTableIdentifier";

// Row identifiers.
NSString* const kPreciseLocationCellId = @"PreciseLocationCellId";

}  // namespace

@implementation BWGLocationViewController {
  // Switch item for toggling precise location.
  TableViewSwitchItem* _preciseLocationSwitchItem;
  // Precise location preference value.
  BOOL _preciseLocationEnabled;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kBWGLocationViewTableIdentifier;
  self.title = l10n_util::GetNSString(IDS_IOS_BWG_LOCATION_TITLE);
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierLocation];

  _preciseLocationSwitchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeLocation];
  _preciseLocationSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_BWG_LOCATION_SWITCH_TITLE);
  _preciseLocationSwitchItem.target = self;
  _preciseLocationSwitchItem.selector =
      @selector(preciseLocationSwitchToggled:);
  _preciseLocationSwitchItem.on = _preciseLocationEnabled;
  _preciseLocationSwitchItem.accessibilityIdentifier = kPreciseLocationCellId;

  TableViewLinkHeaderFooterItem* locationFooterItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeLocationFooter];
  locationFooterItem.text =
      l10n_util::GetNSString(IDS_IOS_BWG_LOCATION_FOOTER_TEXT);

  [model addItem:_preciseLocationSwitchItem
      toSectionWithIdentifier:SectionIdentifierLocation];
  [model setFooter:locationFooterItem
      forSectionWithIdentifier:SectionIdentifierLocation];
}

- (void)setPreciseLocationEnabled:(BOOL)enabled {
  _preciseLocationEnabled = enabled;
  if ([self isViewLoaded]) {
    _preciseLocationSwitchItem.on = _preciseLocationEnabled;
    [self reconfigureCellsForItems:@[ _preciseLocationSwitchItem ]];
  }
}

#pragma mark - Private

// Called from the Precise Location setting's UIControlEventValueChanged.
// Updates underlying precise location sharing pref.
- (void)preciseLocationSwitchToggled:(UISwitch*)switchView {
  [self.mutator setPreciseLocationPref:switchView.isOn];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiLocationSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiLocationSettingsBack"));
}

@end
