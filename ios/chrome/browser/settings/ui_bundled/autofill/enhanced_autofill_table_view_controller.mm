// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/enhanced_autofill_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/utils/autofill_and_passwords_item_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitches = kSectionIdentifierEnumZero,
  SectionIdentifierWhenOn,
  SectionIdentifierThingsToConsider
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeEnhancedAutofillSwitch = kItemTypeEnumZero,
  ItemTypeFooter,
  ItemTypeHeader,
  ItemTypeLabel
};

}  // namespace

@interface EnhancedAutofillTableViewController () <PrefObserverDelegate> {
  raw_ptr<Browser> _browser;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Pref observer to track changes to prefs.
  std::optional<PrefObserverBridge> _prefObserverBridge;
}

@end

@implementation EnhancedAutofillTableViewController

- (instancetype)initWithBrowser:(Browser*)browser {
  CHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE);
    _browser = browser;
    _prefChangeRegistrar.Init(_browser->GetProfile()->GetPrefs());
    _prefObserverBridge.emplace(self);
    // Register to observe any changes on Pref-backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::GetAutofillAiOptInPreferenceKeyName(), &_prefChangeRegistrar);
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitches];
  [model addItem:[self enhancedAutofillSwitchItem]
      toSectionWithIdentifier:SectionIdentifierSwitches];
  [model setFooter:[self enhancedAutofillSwitchFooter]
      forSectionWithIdentifier:SectionIdentifierSwitches];

  [model addSectionWithIdentifier:SectionIdentifierWhenOn];
  [model setHeader:[self whenOnSectionHeader]
      forSectionWithIdentifier:SectionIdentifierWhenOn];
  [model addItem:[self canFillDifficultFieldsItem]
      toSectionWithIdentifier:SectionIdentifierWhenOn];

  [model addSectionWithIdentifier:SectionIdentifierThingsToConsider];
  [model setHeader:[self thingsToConsiderSectionHeader]
      forSectionWithIdentifier:SectionIdentifierThingsToConsider];
  [model addItem:[self dataUsageItem]
      toSectionWithIdentifier:SectionIdentifierThingsToConsider];

  PrefService* prefs = _browser->GetProfile()->GetPrefs();
  if (!autofill::IsAutofillAiAllowedByEnterprisePolicy(prefs)) {
    [model addItem:[self enterpriseManagedLoggingDisabledItem]
        toSectionWithIdentifier:SectionIdentifierThingsToConsider];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("EnhancedAutofillSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("EnhancedAutofillSettingsBack"));
}

#pragma mark - LoadModel Helpers

- (TableViewItem*)enhancedAutofillSwitchItem {
  return EnhancedAutofillSwitchItem(ItemTypeEnhancedAutofillSwitch,
                                    [self isEnhancedAutofillEnabled], self,
                                    @selector(enhancedAutofillSwitchChanged:));
}

- (TableViewHeaderFooterItem*)enhancedAutofillSwitchFooter {
  return EnhancedAutofillSwitchFooter(ItemTypeFooter);
}

- (TableViewHeaderFooterItem*)whenOnSectionHeader {
  return EnhancedAutofillWhenOnSectionHeader(ItemTypeHeader);
}

- (TableViewDetailIconItem*)canFillDifficultFieldsItem {
  return EnhancedAutofillCanFillDifficultFieldsItem(ItemTypeLabel);
}

- (TableViewDetailIconItem*)enterpriseManagedLoggingDisabledItem {
  return EnhancedAutofillEnterpriseManagedLoggingDisabledItem(ItemTypeLabel);
}

- (TableViewHeaderFooterItem*)thingsToConsiderSectionHeader {
  return EnhancedAutofillThingsToConsiderSectionHeader(ItemTypeHeader);
}

- (TableViewDetailIconItem*)dataUsageItem {
  return EnhancedAutofillDataUsageItem(ItemTypeLabel);
}

#pragma mark - Getters and Setter

- (BOOL)isEnhancedAutofillEnabled {
  return autofill::IsEnhancedAutofillEnabled(_browser->GetProfile());
}

- (void)setEnhancedAutofillEnabled:(BOOL)isEnabled {
  autofill::SetEnhancedAutofillEnabled(_browser->GetProfile(), isEnabled);
}

#pragma mark - Switch Callbacks

- (void)enhancedAutofillSwitchChanged:(UISwitch*)switchView {
  BOOL enabled = switchView.on;
  [self setSwitchItemOn:enabled itemType:ItemTypeEnhancedAutofillSwitch];
  [self setEnhancedAutofillEnabled:enabled];
}

#pragma mark - Switch Helpers

// Sets switchItem's state to `on`. It is important that there is only one item
// of `switchItemType` in SectionIdentifierSwitches.
- (void)setSwitchItemOn:(BOOL)on itemType:(ItemType)switchItemType {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:switchItemType
                              sectionIdentifier:SectionIdentifierSwitches];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.on = on;
  [self reconfigureCellsForItems:@[ switchItem ]];
}

// Sets switchItem's enabled status to `enabled` and reconfigures the
// corresponding cell. It is important that there is no more than one item of
// `switchItemType` in SectionIdentifierSwitches.
- (void)setSwitchItemEnabled:(BOOL)enabled itemType:(ItemType)switchItemType {
  TableViewModel* model = self.tableViewModel;

  if (![model hasItemForItemType:switchItemType
               sectionIdentifier:SectionIdentifierSwitches]) {
    return;
  }
  NSIndexPath* switchPath =
      [model indexPathForItemType:switchItemType
                sectionIdentifier:SectionIdentifierSwitches];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  [switchItem setEnabled:enabled];
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - SettingsControllerProtocol

- (void)settingsWillBeDismissed {
  // Remove pref changes registrations.
  _prefChangeRegistrar.RemoveAll();
  // Remove observer bridges.
  _prefObserverBridge.reset();

  _browser = nullptr;
  _settingsAreDismissed = YES;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // If the model hasn't been created yet, no need to update anything.
  if (!self.tableViewModel) {
    return;
  }

  if (preferenceName == autofill::GetAutofillAiOptInPreferenceKeyName()) {
    [self setSwitchItemOn:[self isEnhancedAutofillEnabled]
                 itemType:ItemTypeEnhancedAutofillSwitch];
  }
}

@end
