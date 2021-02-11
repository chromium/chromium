// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"

#include "base/check.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/content_settings/core/common/features.h"
#include "components/handoff/pref_names_ios.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPrivacyTableViewId = @"kPrivacyTableViewId";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPrivacyContent = kSectionIdentifierEnumZero,
  SectionIdentifierWebServices,
  SectionIdentifierIncognitoAuth,

};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeClearBrowsingDataClear = kItemTypeEnumZero,
  // Footer to suggest the user to open Sync and Google services settings.
  ItemTypePrivacyFooter,
  ItemTypeOtherDevicesHandoff,
  ItemTypeIncognitoReauth,
};

// Only used in this class to openn the Sync and Google services settings.
// This link should not be dispatched.
const char kGoogleServicesSettingsURL[] = "settings://open_google_services";

}  // namespace

@interface PrivacyTableViewController () <BooleanObserver,
                                          PrefObserverDelegate> {
  ChromeBrowserState* _browserState;  // weak

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Updatable Items
  TableViewDetailIconItem* _handoffDetailItem;
}

// Browser.
@property(nonatomic, readonly) Browser* browser;

// Accessor for the incognito reauth pref.
@property(nonatomic, strong) PrefBackedBoolean* incognitoReauthPref;

// Switch item for toggling incognito reauth.
@property(nonatomic, strong) SettingsSwitchItem* incognitoReauthItem;

// Authentication module used when the user toggles the biometric auth on.
@property(nonatomic, strong) id<ReauthenticationProtocol> reauthModule;

@end

@implementation PrivacyTableViewController

#pragma mark - Initialization

- (instancetype)initWithBrowser:(Browser*)browser
         reauthenticationModule:(id<ReauthenticationProtocol>)reauthModule {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    _reauthModule = reauthModule;
    _browserState = browser->GetBrowserState();
    self.title =
        l10n_util::GetNSString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY);

    PrefService* prefService = _browserState->GetPrefs();

    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosHandoffToOtherDevices, &_prefChangeRegistrar);

    if (base::FeatureList::IsEnabled(kIncognitoAuthentication)) {
      _incognitoReauthPref = [[PrefBackedBoolean alloc]
          initWithPrefService:GetApplicationContext()->GetLocalState()
                     prefName:prefs::kIncognitoAuthenticationSetting];
      [_incognitoReauthPref setObserver:self];
    }
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPrivacyTableViewId;

  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate privacyTableViewControllerWasRemoved:self];
  }
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierPrivacyContent];
  [model addSectionWithIdentifier:SectionIdentifierWebServices];
  if (base::FeatureList::IsEnabled(kIncognitoAuthentication)) {
    [model addSectionWithIdentifier:SectionIdentifierIncognitoAuth];
  }

  // Clear Browsing item.
  [model addItem:[self clearBrowsingDetailItem]
      toSectionWithIdentifier:SectionIdentifierPrivacyContent];

  [model setFooter:[self showPrivacyFooterItem]
      forSectionWithIdentifier:SectionIdentifierPrivacyContent];

  // Web Services item.
  [model addItem:[self handoffDetailItem]
      toSectionWithIdentifier:SectionIdentifierWebServices];

  // Do not show the incognito authentication setting when Incongito mode is
  // disabled.
  if (base::FeatureList::IsEnabled(kIncognitoAuthentication) &&
      !IsIncognitoModeDisabled(_browserState->GetPrefs())) {
    // Incognito authentication item.
    [model addItem:self.incognitoReauthItem
        toSectionWithIdentifier:SectionIdentifierIncognitoAuth];
  }
}

#pragma mark - Model Objects

- (TableViewItem*)handoffDetailItem {
  NSString* detailText =
      _browserState->GetPrefs()->GetBoolean(prefs::kIosHandoffToOtherDevices)
          ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
          : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _handoffDetailItem = [self
           detailItemWithType:ItemTypeOtherDevicesHandoff
                      titleId:IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES
                   detailText:detailText
      accessibilityIdentifier:kSettingsHandoffCellId];

  return _handoffDetailItem;
}

// Creates TableViewHeaderFooterItem instance to show a link to open the Sync
// and Google services settings.
- (TableViewHeaderFooterItem*)showPrivacyFooterItem {
  TableViewLinkHeaderFooterItem* showPrivacyFooterItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypePrivacyFooter];
  showPrivacyFooterItem.text =
      signin::IsMobileIdentityConsistencyEnabled()
          ? l10n_util::GetNSString(IDS_IOS_PRIVACY_GOOGLE_SERVICES_FOOTER)
          : l10n_util::GetNSString(
                IDS_IOS_OPTIONS_PRIVACY_GOOGLE_SERVICES_FOOTER);

  showPrivacyFooterItem.linkURL = GURL(kGoogleServicesSettingsURL);

  return showPrivacyFooterItem;
}

- (TableViewItem*)clearBrowsingDetailItem {
  return [self detailItemWithType:ItemTypeClearBrowsingDataClear
                          titleId:IDS_IOS_CLEAR_BROWSING_DATA_TITLE
                       detailText:nil
          accessibilityIdentifier:kSettingsClearBrowsingDataCellId];
}

- (SettingsSwitchItem*)incognitoReauthItem {
  DCHECK(base::FeatureList::IsEnabled(kIncognitoAuthentication));

  if (_incognitoReauthItem) {
    return _incognitoReauthItem;
  }
  _incognitoReauthItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeIncognitoReauth];
  _incognitoReauthItem.text =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_REAUTH_SETTING_NAME);
  _incognitoReauthItem.on = self.incognitoReauthPref.value;
  return _incognitoReauthItem;
}

- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                       titleId:(NSInteger)titleId
                                    detailText:(NSString*)detailText
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = l10n_util::GetNSString(titleId);
  detailItem.detailText = detailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;

  return detailItem;
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobilePrivacySettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobilePrivacySettingsBack"));
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
  TableViewLinkHeaderFooterView* footer =
      base::mac::ObjCCast<TableViewLinkHeaderFooterView>(footerView);
  if (footer) {
    footer.delegate = self;
  }
  return footerView;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeOtherDevicesHandoff:
      [self.handler showHandoff];
      break;
    case ItemTypeClearBrowsingDataClear:
      [self.handler showClearBrowsingData];
      break;
    default:
      break;
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (itemType == ItemTypeIncognitoReauth) {
    SettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchTapped:)
                    forControlEvents:UIControlEventTouchUpInside];
  }

  return cell;
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
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  // Update the cell.
  self.incognitoReauthItem.on = self.incognitoReauthPref.value;
  [self reconfigureCellsForItems:@[ self.incognitoReauthItem ]];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(GURL)URL {
  if (URL == GURL(kGoogleServicesSettingsURL)) {
    // kGoogleServicesSettingsURL is not a realy link. It should be handled
    // with a special case.
    [self.dispatcher showGoogleServicesSettingsFromViewController:self];
  } else {
    [super view:view didTapLinkURL:URL];
  }
}

#pragma mark - private

// Called from the reauthentication setting's UIControlEventTouchUpInside.
// When this is called, |switchView| already has the updated value:
// If the switch was off, and user taps it, when this method is called,
// switchView.on is YES.
- (void)switchTapped:(UISwitch*)switchView {
  if (switchView.isOn && ![self.reauthModule canAttemptReauth]) {
    // TODO(crbug.com/1148818): add error message here or maybe even disable
    // the switch?
    switchView.on = false;
    return;
  }

  __weak PrivacyTableViewController* weakSelf = self;
  [self.reauthModule
      attemptReauthWithLocalizedReason:
          l10n_util::GetNSString(
              IDS_IOS_INCOGNITO_REAUTH_SET_UP_SYSTEM_DIALOG_REASON)
                  canReusePreviousAuth:false
                               handler:^(ReauthenticationResult result) {
                                 BOOL enabled = switchView.on;
                                 if (result !=
                                     ReauthenticationResult::kSuccess) {
                                   // Revert the switch if authentication wasn't
                                   // successful.
                                   enabled = !enabled;
                                 }
                                 [switchView setOn:enabled animated:YES];
                                 weakSelf.incognitoReauthPref.value = enabled;
                               }];
}

@end
