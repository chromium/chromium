// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/handoff_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/handoff/pref_names_ios.h"
#import "components/prefs/pref_member.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitch = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSwitch = kItemTypeEnumZero,
  ItemTypeFooter,
};

}  // namespace

@interface HandoffTableViewController () <BooleanObserver,
                                          SettingsControllerProtocol> {
  // Pref for whether Handoff is enabled.
  PrefBackedBoolean* _handoffEnabled;

  // Item for displaying handoff switch.
  TableViewSwitchItem* _handoffSwitchItem;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@end

@implementation HandoffTableViewController

#pragma mark - Initialization

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  DCHECK(profile);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_OPTIONS_CONTINUITY_LABEL);
    _handoffEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:profile->GetPrefs()
                   prefName:prefs::kIosHandoffToOtherDevices];
    _handoffEnabled.observer = self;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedSectionFooterHeight = 70;

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitch];
  _handoffSwitchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeSwitch];
  _handoffSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES);
  _handoffSwitchItem.on = _handoffEnabled.value;
  [model addItem:_handoffSwitchItem
      toSectionWithIdentifier:SectionIdentifierSwitch];

  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES_DETAILS);
  [model setFooter:footer forSectionWithIdentifier:SectionIdentifierSwitch];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (itemType == ItemTypeSwitch) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileHandoffSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileHandoffSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // Stop observable prefs.
  [_handoffEnabled stop];
  _handoffEnabled.observer = nil;
  _handoffEnabled = nil;

  _settingsAreDismissed = YES;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  // Update the cell.
  _handoffSwitchItem.on = _handoffEnabled.value;
  [self reconfigureCellsForItems:@[ _handoffSwitchItem ]];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSHandoffSettingsCloseWithSwipe"));
}

#pragma mark - Private

- (void)switchChanged:(UISwitch*)switchView {
  _handoffEnabled.value = switchView.isOn;
}

@end
