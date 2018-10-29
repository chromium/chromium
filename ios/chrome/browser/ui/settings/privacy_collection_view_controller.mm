// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy_collection_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/google/core/common/google_util.h"
#include "components/handoff/pref_names_ios.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/payments/core/payment_prefs.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/autofill/autofill_controller.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/accounts_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_detail_item.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/dataplan_usage_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/handoff_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPrivacyCollectionViewId = @"kPrivacyCollectionViewId";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierOtherDevices = kSectionIdentifierEnumZero,
  SectionIdentifierWebServices,
  SectionIdentifierWebServicesFooter,
  SectionIdentifierCanMakePayment,
  SectionIdentifierClearBrowsingData,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeOtherDevicesHeader = kItemTypeEnumZero,
  ItemTypeOtherDevicesHandoff,
  ItemTypeWebServicesHeader,
  ItemTypeWebServicesFooter,
  ItemTypeWebServicesShowSuggestions,
  ItemTypeWebServicesSendUsageData,
  ItemTypeCanMakePaymentSwitch,
  ItemTypeClearBrowsingDataClear,
};

}  // namespace

@interface PrivacyCollectionViewController ()<BooleanObserver,
                                              PrefObserverDelegate> {
  ios::ChromeBrowserState* _browserState;  // weak
  PrefBackedBoolean* _suggestionsEnabled;
  // The item related to the switch for the show suggestions setting.
  LegacySettingsSwitchItem* _showSuggestionsItem;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  PrefChangeRegistrar _prefChangeRegistrarApplicationContext;

  // Updatable Items
  LegacySettingsDetailItem* _handoffDetailItem;
  LegacySettingsDetailItem* _sendUsageDetailItem;
}

// Initialization methods for various model items.
- (CollectionViewItem*)handoffDetailItem;
- (LegacySettingsSwitchItem*)showSuggestionsSwitchItem;
- (CollectionViewItem*)showSuggestionsFooterItem;
- (CollectionViewItem*)clearBrowsingDetailItem;
- (CollectionViewItem*)canMakePaymentItem;
- (CollectionViewItem*)sendUsageDetailItem;

@end

@implementation PrivacyCollectionViewController

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    _browserState = browserState;
    self.title =
        l10n_util::GetNSString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY);
    self.collectionViewAccessibilityIdentifier = kPrivacyCollectionViewId;
    if (!unified_consent::IsUnifiedConsentFeatureEnabled()) {
      // When unified consent flag is enabled, the suggestion setting is
      // available in the "Google Services and sync" settings.
      _suggestionsEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_browserState->GetPrefs()
                     prefName:prefs::kSearchSuggestEnabled];
      [_suggestionsEnabled setObserver:self];
    }

    PrefService* prefService = _browserState->GetPrefs();

    _prefChangeRegistrar.Init(prefService);
    _prefChangeRegistrarApplicationContext.Init(
        GetApplicationContext()->GetLocalState());
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosHandoffToOtherDevices, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        metrics::prefs::kMetricsReportingEnabled,
        &_prefChangeRegistrarApplicationContext);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kMetricsReportingWifiOnly,
        &_prefChangeRegistrarApplicationContext);

    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];

  CollectionViewModel* model = self.collectionViewModel;

  // Other Devices Section
  [model addSectionWithIdentifier:SectionIdentifierOtherDevices];
  SettingsTextItem* otherDevicesHeader =
      [[SettingsTextItem alloc] initWithType:ItemTypeOtherDevicesHeader];
  otherDevicesHeader.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_CONTINUITY_LABEL);
  otherDevicesHeader.textColor = [[MDCPalette greyPalette] tint500];
  [model setHeader:otherDevicesHeader
      forSectionWithIdentifier:SectionIdentifierOtherDevices];
  [model addItem:[self handoffDetailItem]
      toSectionWithIdentifier:SectionIdentifierOtherDevices];

  if (!unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // Add "Web services" section only if the unified consent is disabled.
    // Otherwise the metrics reporting and show suggestions feature are
    // available in the Google services settings.
    [model addSectionWithIdentifier:SectionIdentifierWebServices];
    SettingsTextItem* webServicesHeader =
        [[SettingsTextItem alloc] initWithType:ItemTypeWebServicesHeader];
    webServicesHeader.text =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_WEB_SERVICES_LABEL);
    webServicesHeader.textColor = [[MDCPalette greyPalette] tint500];
    [model setHeader:webServicesHeader
        forSectionWithIdentifier:SectionIdentifierWebServices];
    // When unified consent flag is enabled, the show suggestions feature and
    // metrics reporting feature are available in the "Google Services and sync"
    // settings.
    _showSuggestionsItem = [self showSuggestionsSwitchItem];
    [model addItem:_showSuggestionsItem
        toSectionWithIdentifier:SectionIdentifierWebServices];
    [model addItem:[self sendUsageDetailItem]
        toSectionWithIdentifier:SectionIdentifierWebServices];
    // Footer Section
    [model addSectionWithIdentifier:SectionIdentifierWebServicesFooter];
    [model addItem:[self showSuggestionsFooterItem]
        toSectionWithIdentifier:SectionIdentifierWebServicesFooter];
  }

  // CanMakePayment Section
  [model addSectionWithIdentifier:SectionIdentifierCanMakePayment];
  [model addItem:[self canMakePaymentItem]
      toSectionWithIdentifier:SectionIdentifierCanMakePayment];

  // Clear Browsing Section
  [model addSectionWithIdentifier:SectionIdentifierClearBrowsingData];
  [model addItem:[self clearBrowsingDetailItem]
      toSectionWithIdentifier:SectionIdentifierClearBrowsingData];
}

#pragma mark - Model Objects

- (CollectionViewItem*)handoffDetailItem {
  NSString* detailText =
      _browserState->GetPrefs()->GetBoolean(prefs::kIosHandoffToOtherDevices)
          ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
          : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _handoffDetailItem =
      [self detailItemWithType:ItemTypeOtherDevicesHandoff
                       titleId:IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES
                    detailText:detailText];

  return _handoffDetailItem;
}

- (LegacySettingsSwitchItem*)showSuggestionsSwitchItem {
  LegacySettingsSwitchItem* showSuggestionsSwitchItem =
      [[LegacySettingsSwitchItem alloc]
          initWithType:ItemTypeWebServicesShowSuggestions];
  showSuggestionsSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_SEARCH_URL_SUGGESTIONS);
  showSuggestionsSwitchItem.on = [_suggestionsEnabled value];

  return showSuggestionsSwitchItem;
}

- (CollectionViewItem*)showSuggestionsFooterItem {
  CollectionViewFooterItem* showSuggestionsFooterItem =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeWebServicesFooter];
  showSuggestionsFooterItem.cellStyle = CollectionViewCellStyle::kUIKit;
  showSuggestionsFooterItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRIVACY_FOOTER);
  showSuggestionsFooterItem.linkURL = google_util::AppendGoogleLocaleParam(
      GURL(kPrivacyLearnMoreURL),
      GetApplicationContext()->GetApplicationLocale());
  showSuggestionsFooterItem.linkDelegate = self;

  return showSuggestionsFooterItem;
}

- (CollectionViewItem*)clearBrowsingDetailItem {
  return [self detailItemWithType:ItemTypeClearBrowsingDataClear
                          titleId:IDS_IOS_CLEAR_BROWSING_DATA_TITLE
                       detailText:nil];
}

- (CollectionViewItem*)canMakePaymentItem {
  LegacySettingsSwitchItem* canMakePaymentItem =
      [[LegacySettingsSwitchItem alloc]
          initWithType:ItemTypeCanMakePaymentSwitch];
  canMakePaymentItem.text =
      l10n_util::GetNSString(IDS_SETTINGS_CAN_MAKE_PAYMENT_TOGGLE_LABEL);
  canMakePaymentItem.on = [self isCanMakePaymentEnabled];
  return canMakePaymentItem;
}

- (BOOL)isCanMakePaymentEnabled {
  return _browserState->GetPrefs()->GetBoolean(
      payments::kCanMakePaymentEnabled);
}

- (void)setCanMakePaymentEnabled:(BOOL)isEnabled {
  _browserState->GetPrefs()->SetBoolean(payments::kCanMakePaymentEnabled,
                                        isEnabled);
}

- (CollectionViewItem*)sendUsageDetailItem {
  NSString* detailText = [DataplanUsageTableViewController
      currentLabelForPreference:GetApplicationContext()->GetLocalState()
                       basePref:metrics::prefs::kMetricsReportingEnabled
                       wifiPref:prefs::kMetricsReportingWifiOnly];
  _sendUsageDetailItem =
      [self detailItemWithType:ItemTypeWebServicesSendUsageData
                       titleId:IDS_IOS_OPTIONS_SEND_USAGE_DATA
                    detailText:detailText];

  return _sendUsageDetailItem;
}

- (LegacySettingsDetailItem*)detailItemWithType:(NSInteger)type
                                        titleId:(NSInteger)titleId
                                     detailText:(NSString*)detailText {
  LegacySettingsDetailItem* detailItem =
      [[LegacySettingsDetailItem alloc] initWithType:type];
  detailItem.text = l10n_util::GetNSString(titleId);
  detailItem.detailText = detailText;
  detailItem.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;

  return detailItem;
}

#pragma mark UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeWebServicesShowSuggestions) {
    LegacySettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(showSuggestionsToggled:)
                    forControlEvents:UIControlEventValueChanged];
  } else if (itemType == ItemTypeCanMakePaymentSwitch) {
    LegacySettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(canMakePaymentSwitchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }

  return cell;
}

#pragma mark UICollectionViewDelegate
- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];

  // Items that push a new view controller.
  UIViewController<SettingsRootViewControlling>* controller;

  switch (itemType) {
    case ItemTypeOtherDevicesHandoff:
      controller = [[HandoffCollectionViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case ItemTypeWebServicesSendUsageData:
      controller = [[DataplanUsageTableViewController alloc]
          initWithPrefs:GetApplicationContext()->GetLocalState()
               basePref:metrics::prefs::kMetricsReportingEnabled
               wifiPref:prefs::kMetricsReportingWifiOnly
                  title:l10n_util::GetNSString(
                            IDS_IOS_OPTIONS_SEND_USAGE_DATA)];
      break;
    case ItemTypeClearBrowsingDataClear:
      controller = [[ClearBrowsingDataCollectionViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case ItemTypeCanMakePaymentSwitch:
    case ItemTypeWebServicesShowSuggestions:
    default:
      break;
  }

  if (controller) {
    controller.dispatcher = self.dispatcher;
    [self.navigationController pushViewController:controller animated:YES];
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];

  if (sectionIdentifier == SectionIdentifierWebServicesFooter) {
    return YES;
  }
  return NO;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeWebServicesFooter:
    case ItemTypeWebServicesShowSuggestions:
    case ItemTypeCanMakePaymentSwitch:
      return YES;
    default:
      return NO;
  }
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  if (item.type == ItemTypeWebServicesFooter ||
      item.type == ItemTypeCanMakePaymentSwitch) {
    return [MDCCollectionViewCell
        cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                           forItem:item];
  }

  return MDCCellDefaultOneLineHeight;
}

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];

  if (sectionIdentifier == SectionIdentifierWebServicesFooter) {
    return MDCCollectionViewCellStyleDefault;
  }

  return self.styler.cellStyle;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _suggestionsEnabled);

  // Update the item.
  _showSuggestionsItem.on = [_suggestionsEnabled value];

  // Update the cell.
  [self reconfigureCellsForItems:@[ _showSuggestionsItem ]];
}

#pragma mark - Actions

- (void)showSuggestionsToggled:(UISwitch*)sender {
  NSIndexPath* switchPath = [self.collectionViewModel
      indexPathForItemType:ItemTypeWebServicesShowSuggestions
         sectionIdentifier:SectionIdentifierWebServices];

  LegacySettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<LegacySettingsSwitchItem>(
          [self.collectionViewModel itemAtIndexPath:switchPath]);
  LegacySettingsSwitchCell* switchCell =
      base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(
          [self.collectionView cellForItemAtIndexPath:switchPath]);

  if (switchCell.switchView.isOn) {
    base::RecordAction(base::UserMetricsAction(
        "ContentSuggestions.RemoteSuggestionsPreferenceOn"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "ContentSuggestions.RemoteSuggestionsPreferenceOff"));
  }

  DCHECK_EQ(switchCell.switchView, sender);
  BOOL isOn = switchCell.switchView.isOn;
  switchItem.on = isOn;
  [_suggestionsEnabled setValue:isOn];
}

- (void)canMakePaymentSwitchChanged:(UISwitch*)sender {
  NSIndexPath* switchPath = [self.collectionViewModel
      indexPathForItemType:ItemTypeCanMakePaymentSwitch
         sectionIdentifier:SectionIdentifierCanMakePayment];

  LegacySettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<LegacySettingsSwitchItem>(
          [self.collectionViewModel itemAtIndexPath:switchPath]);
  LegacySettingsSwitchCell* switchCell =
      base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(
          [self.collectionView cellForItemAtIndexPath:switchPath]);

  DCHECK_EQ(switchCell.switchView, sender);
  switchItem.on = sender.isOn;
  [self setCanMakePaymentEnabled:sender.isOn];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kIosHandoffToOtherDevices) {
    NSString* detailText =
        _browserState->GetPrefs()->GetBoolean(prefs::kIosHandoffToOtherDevices)
            ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
            : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _handoffDetailItem.detailText = detailText;
    [self reconfigureCellsForItems:@[ _handoffDetailItem ]];
    return;
  }

  if (preferenceName == metrics::prefs::kMetricsReportingEnabled ||
      preferenceName == prefs::kMetricsReportingWifiOnly) {
    NSString* detailText = [DataplanUsageTableViewController
        currentLabelForPreference:GetApplicationContext()->GetLocalState()
                         basePref:metrics::prefs::kMetricsReportingEnabled
                         wifiPref:prefs::kMetricsReportingWifiOnly];

    _sendUsageDetailItem.detailText = detailText;

    [self reconfigureCellsForItems:@[ _sendUsageDetailItem ]];
    return;
  }
}

@end
