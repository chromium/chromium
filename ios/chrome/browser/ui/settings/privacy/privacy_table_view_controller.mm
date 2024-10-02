// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"

#import <LocalAuthentication/LocalAuthentication.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/content_settings/core/common/features.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/handoff/pref_names_ios.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_features.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/incognito_interstitial/ui_bundled/incognito_interstitial_constants.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/elements/info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/elements/supervised_user_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/features.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/web/model/features.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPrivacyContent = kSectionIdentifierEnumZero,
  SectionIdentifierSafeBrowsing,
  SectionIdentifierHTTPSOnlyMode,
  SectionIdentifierWebServices,
  SectionIdentifierIncognitoAuth,
  SectionIdentifierIncognitoInterstitial,
  SectionIdentifierLockdownMode,
  SectionIdentifierPrivacyGuide,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeClearBrowsingDataClear = kItemTypeEnumZero,
  // Footer to suggest the user to open Sync and Google services settings.
  ItemTypePrivacyFooter,
  ItemTypeOtherDevicesHandoff,
  ItemTypeIncognitoReauth,
  ItemTypeIncognitoReauthDisabled,
  ItemTypePrivacySafeBrowsing,
  ItemTypeHTTPSOnlyMode,
  ItemTypeIncognitoInterstitial,
  ItemTypeIncognitoInterstitialDisabled,
  ItemTypeLockdownMode,
  ItemTypePrivacyGuide,
};

// Used to open the Sync and Google Services settings.
// These links should not be dispatched.
const char kGoogleServicesSettingsURL[] = "settings://open_google_services";
const char kSyncSettingsURL[] = "settings://open_sync";

}  // namespace

@interface PrivacyTableViewController () <BooleanObserver,
                                          PrefObserverDelegate,
                                          PopoverLabelViewControllerDelegate,
                                          SyncObserverModelBridge> {
  raw_ptr<ProfileIOS> _profile;  // weak

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Sync Observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;

  // Updatable Items.
  TableViewDetailIconItem* _handoffDetailItem;
  // Safe Browsing item.
  TableViewDetailIconItem* _safeBrowsingDetailItem;
  // Locdown Mode item.
  TableViewDetailIconItem* _lockdownModeDetailItem;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // Registrar for local pref changes notifications.
  PrefChangeRegistrar _localStateChangeRegistrar;
}

// Accessor for the incognito reauth pref.
@property(nonatomic, strong) PrefBackedBoolean* incognitoReauthPref;

// Switch item for toggling incognito reauth.
@property(nonatomic, strong) TableViewSwitchItem* incognitoReauthItem;

// Authentication module used when the user toggles the biometric auth on.
@property(nonatomic, strong) id<ReauthenticationProtocol> reauthModule;

// Accessor for the HTTPS-Only Mode pref.
@property(nonatomic, strong) PrefBackedBoolean* HTTPSOnlyModePref;

// The item related to the switch for the "HTTPS Only Mode" setting.
@property(nonatomic, strong) TableViewSwitchItem* HTTPSOnlyModeItem;

// Accessor for the Incognito interstitial pref.
@property(nonatomic, strong) PrefBackedBoolean* incognitoInterstitialPref;

// The item related to the Incognito interstitial setting.
@property(nonatomic, strong) TableViewSwitchItem* incognitoInterstitialItem;

@end

@implementation PrivacyTableViewController

#pragma mark - Initialization

- (instancetype)initWithBrowser:(Browser*)browser
         reauthenticationModule:(id<ReauthenticationProtocol>)reauthModule {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _reauthModule = reauthModule;
    _profile = browser->GetProfile();
    self.title = l10n_util::GetNSString(IDS_IOS_SETTINGS_PRIVACY_TITLE);

    PrefService* prefService = _profile->GetPrefs();

    _prefChangeRegistrar.Init(prefService);
    _localStateChangeRegistrar.Init(GetApplicationContext()->GetLocalState());

    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosHandoffToOtherDevices, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kSafeBrowsingEnabled, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kSafeBrowsingEnhanced, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kBrowserLockdownModeEnabled, &_localStateChangeRegistrar);
    _syncObserver.reset(new SyncObserverBridge(
        self, SyncServiceFactory::GetForProfile(_profile)));

    _incognitoReauthPref = [[PrefBackedBoolean alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()
                   prefName:prefs::kIncognitoAuthenticationSetting];
    [_incognitoReauthPref setObserver:self];

    _HTTPSOnlyModePref = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::kHttpsOnlyModeEnabled];
    [_HTTPSOnlyModePref setObserver:self];

    _incognitoInterstitialPref = [[PrefBackedBoolean alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()
                   prefName:prefs::kIncognitoInterstitialEnabled];
    [_incognitoInterstitialPref setObserver:self];
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

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed)
    return;

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierPrivacyContent];
  if (IsPrivacyGuideIosEnabled()) {
    [model addSectionWithIdentifier:SectionIdentifierPrivacyGuide];
  }
  [model addSectionWithIdentifier:SectionIdentifierSafeBrowsing];

  if (base::FeatureList::IsEnabled(
          security_interstitials::features::kHttpsOnlyMode)) {
    [model addSectionWithIdentifier:SectionIdentifierHTTPSOnlyMode];
    [model addItem:self.HTTPSOnlyModeItem
        toSectionWithIdentifier:SectionIdentifierHTTPSOnlyMode];
  }

  [model addSectionWithIdentifier:SectionIdentifierWebServices];
  [model addSectionWithIdentifier:SectionIdentifierIncognitoAuth];
  [model addSectionWithIdentifier:SectionIdentifierIncognitoInterstitial];
  [model addSectionWithIdentifier:SectionIdentifierLockdownMode];

  // Clear Browsing item.
  [model addItem:[self clearBrowsingDetailItem]
      toSectionWithIdentifier:SectionIdentifierPrivacyContent];

  // Privacy Guide item.
  if (IsPrivacyGuideIosEnabled()) {
    [model addItem:[self privacyGuideDetailItem]
        toSectionWithIdentifier:SectionIdentifierPrivacyGuide];
  }

  // Privacy Safe Browsing item.
  [model addItem:[self safeBrowsingDetailItem]
      toSectionWithIdentifier:SectionIdentifierSafeBrowsing];

  // Web Services item.
  [model addItem:[self handoffDetailItem]
      toSectionWithIdentifier:SectionIdentifierWebServices];

  // Incognito reauth item is added. If Incognito mode is disabled, or device
  // authentication is not supported, a disabled version is shown instead with
  // relevant information as a popover.
  TableViewItem* incognitoReauthItem =
      (IsIncognitoModeDisabled(_profile->GetPrefs()) ||
       ![self deviceSupportsAuthentication])
          ? self.incognitoReauthItemDisabled
          : self.incognitoReauthItem;
  [model addItem:incognitoReauthItem
      toSectionWithIdentifier:SectionIdentifierIncognitoAuth];

  // Show "Ask to Open Links from Other Apps in Incognito" setting.
  // Incognito interstitial item is added. If Incognito mode is
  // disabled or forced, a disabled version is shown with information
  // to learn more.
  TableViewItem* incognitoInterstitialItem =
      (IsIncognitoModeDisabled(_profile->GetPrefs()) ||
       IsIncognitoModeForced(_profile->GetPrefs()))
          ? self.incognitoInterstitialItemDisabled
          : self.incognitoInterstitialItem;
  [model addItem:incognitoInterstitialItem
      toSectionWithIdentifier:SectionIdentifierIncognitoInterstitial];

  // Lockdown Mode item.
  [model addItem:[self lockdownModeDetailItem]
      toSectionWithIdentifier:SectionIdentifierLockdownMode];
  [model setFooter:[self showPrivacyFooterItem]
      forSectionWithIdentifier:SectionIdentifierLockdownMode];
}

#pragma mark - Model Objects

- (TableViewSwitchItem*)HTTPSOnlyModeItem {
  if (!_HTTPSOnlyModeItem) {
    _HTTPSOnlyModeItem =
        [[TableViewSwitchItem alloc] initWithType:ItemTypeHTTPSOnlyMode];

    _HTTPSOnlyModeItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_HTTPS_ONLY_MODE_TITLE);
    _HTTPSOnlyModeItem.detailText =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_HTTPS_ONLY_MODE_DESCRIPTION);
    _HTTPSOnlyModeItem.on = [self.HTTPSOnlyModePref value];
    _HTTPSOnlyModeItem.accessibilityIdentifier = kSettingsHttpsOnlyModeCellId;
  }
  return _HTTPSOnlyModeItem;
}

- (TableViewSwitchItem*)incognitoInterstitialItem {
  if (!_incognitoInterstitialItem) {
    _incognitoInterstitialItem = [[TableViewSwitchItem alloc]
        initWithType:ItemTypeIncognitoInterstitial];
    _incognitoInterstitialItem.text =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_INCOGNITO_INTERSTITIAL);
    _incognitoInterstitialItem.on = self.incognitoInterstitialPref.value;
    _incognitoInterstitialItem.enabled = YES;
    _incognitoInterstitialItem.accessibilityIdentifier =
        kSettingsIncognitoInterstitialId;
  }
  return _incognitoInterstitialItem;
}

- (TableViewInfoButtonItem*)incognitoInterstitialItemDisabled {
  TableViewInfoButtonItem* itemDisabled = [[TableViewInfoButtonItem alloc]
      initWithType:ItemTypeIncognitoInterstitialDisabled];
  itemDisabled.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_INCOGNITO_INTERSTITIAL);
  itemDisabled.statusText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  itemDisabled.accessibilityIdentifier =
      kSettingsIncognitoInterstitialDisabledId;
  itemDisabled.iconTintColor = [UIColor colorNamed:kGrey300Color];
  itemDisabled.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return itemDisabled;
}

- (TableViewItem*)handoffDetailItem {
  NSString* detailText =
      _profile->GetPrefs()->GetBoolean(prefs::kIosHandoffToOtherDevices)
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

  NSString* privacyFooterText;

  syncer::SyncService* syncService =
      SyncServiceFactory::GetInstance()->GetForProfile(_profile);

  NSMutableArray* urls = [[NSMutableArray alloc] init];
  // TODO(crbug.com/40066949): Remove IsSyncFeatureEnabled() usage after kSync
  // users are migrated to kSignin in phase 3. See ConsentLevel::kSync for more
  // details.
  if (syncService->IsSyncFeatureEnabled()) {
    privacyFooterText =
        l10n_util::GetNSString(IDS_IOS_PRIVACY_SYNC_AND_GOOGLE_SERVICES_FOOTER);
    [urls addObject:[[CrURL alloc] initWithGURL:GURL(kSyncSettingsURL)]];
  } else {
    if (!syncService->GetAccountInfo().IsEmpty()) {
      // Footer for signed in users.
      privacyFooterText = l10n_util::GetNSString(
          IDS_IOS_PRIVACY_ACCOUNT_SETTINGS_AND_GOOGLE_SERVICES_FOOTER);
      [urls addObject:[[CrURL alloc] initWithGURL:GURL(kSyncSettingsURL)]];
    } else {
      // Footer for signed out users.
      privacyFooterText =
          l10n_util::GetNSString(IDS_IOS_PRIVACY_SIGNED_OUT_FOOTER);
    }
  }
  [urls
      addObject:[[CrURL alloc] initWithGURL:GURL(kGoogleServicesSettingsURL)]];

  showPrivacyFooterItem.text = privacyFooterText;
  showPrivacyFooterItem.urls = urls;
  return showPrivacyFooterItem;
}

- (void)updatePrivacyFooterItem {
  // The user might sign out from account settings, and thus the footer should
  // change.
  DCHECK([self.tableViewModel
      hasSectionForSectionIdentifier:SectionIdentifierLockdownMode]);
  [self.tableViewModel setFooter:[self showPrivacyFooterItem]
        forSectionWithIdentifier:SectionIdentifierLockdownMode];
  NSUInteger sectionIndex = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierLockdownMode];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                withRowAnimation:UITableViewRowAnimationNone];
}

- (TableViewItem*)clearBrowsingDetailItem {
  return [self detailItemWithType:ItemTypeClearBrowsingDataClear
                          titleId:IDS_IOS_CLEAR_BROWSING_DATA_TITLE
                       detailText:nil
          accessibilityIdentifier:kSettingsClearBrowsingDataCellId];
}

- (TableViewItem*)safeBrowsingDetailItem {
  NSString* detailText = [self safeBrowsingDetailText];
  _safeBrowsingDetailItem =
      [self detailItemWithType:ItemTypePrivacySafeBrowsing
                          titleId:IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE
                       detailText:detailText
          accessibilityIdentifier:kSettingsPrivacySafeBrowsingCellId];
  return _safeBrowsingDetailItem;
}

- (TableViewItem*)lockdownModeDetailItem {
  NSString* detailText = GetApplicationContext()->GetLocalState()->GetBoolean(
                             prefs::kBrowserLockdownModeEnabled)
                             ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                             : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _lockdownModeDetailItem =
      [self detailItemWithType:ItemTypeLockdownMode
                          titleId:IDS_IOS_LOCKDOWN_MODE_TITLE
                       detailText:detailText
          accessibilityIdentifier:kPrivacyLockdownModeCellId];
  return _lockdownModeDetailItem;
}

- (TableViewItem*)privacyGuideDetailItem {
  return [self detailItemWithType:ItemTypePrivacyGuide
                          titleId:IDS_IOS_PRIVACY_GUIDE_TITLE
                       detailText:nil
          accessibilityIdentifier:kSettingsPrivacyGuideCellId];
}

- (TableViewSwitchItem*)incognitoReauthItem {
  if (_incognitoReauthItem) {
    return _incognitoReauthItem;
  }
  _incognitoReauthItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeIncognitoReauth];
  _incognitoReauthItem.text =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_REAUTH_SETTING_NAME);
  _incognitoReauthItem.on = self.incognitoReauthPref.value;
  _incognitoReauthItem.enabled = YES;
  return _incognitoReauthItem;
}

- (TableViewInfoButtonItem*)incognitoReauthItemDisabled {
  TableViewInfoButtonItem* itemDisabled = [[TableViewInfoButtonItem alloc]
      initWithType:ItemTypeIncognitoReauthDisabled];
  itemDisabled.text =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_REAUTH_SETTING_NAME);
  itemDisabled.statusText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  itemDisabled.iconTintColor = [UIColor colorNamed:kGrey300Color];
  itemDisabled.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return itemDisabled;
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

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // Stop observable prefs.
  [_incognitoReauthPref stop];
  _incognitoReauthPref.observer = nil;
  _incognitoReauthPref = nil;
  [_HTTPSOnlyModePref stop];
  _HTTPSOnlyModePref.observer = nil;
  _HTTPSOnlyModePref = nil;
  [_incognitoInterstitialPref stop];
  _incognitoInterstitialPref.observer = nil;
  _incognitoInterstitialPref = nil;

  // Remove pref changes registrations.
  _prefChangeRegistrar.RemoveAll();
  _localStateChangeRegistrar.Reset();

  // Remove observer bridges.
  _prefObserverBridge.reset();

  // Remove sync observer.
  _syncObserver.reset();

  // Clear C++ ivars.
  _profile = nullptr;

  _settingsAreDismissed = YES;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
  TableViewLinkHeaderFooterView* footer =
      base::apple::ObjCCast<TableViewLinkHeaderFooterView>(footerView);
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
    case ItemTypePrivacySafeBrowsing:
      base::RecordAction(base::UserMetricsAction(
          "SafeBrowsing.Settings.ShowedFromParentSettings"));
      [self.handler showSafeBrowsing];
      break;
    case ItemTypeLockdownMode:
      [self.handler showLockdownMode];
      break;
    case ItemTypePrivacyGuide:
      [self.handler showPrivacyGuide];
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
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchTapped:)
                    forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeIncognitoReauthDisabled) {
    TableViewInfoButtonCell* managedCell =
        base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
    [managedCell.trailingButton
               addTarget:self
                  action:@selector(didTapIncognitoReauthDisabledInfoButton:)
        forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeHTTPSOnlyMode) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(HTTPSOnlyModeTapped:)
                    forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeIncognitoInterstitial) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView
               addTarget:self
                  action:@selector(incognitoInterstitialSwitchTapped:)
        forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeIncognitoInterstitialDisabled) {
    TableViewInfoButtonCell* managedCell =
        base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
    [managedCell.trailingButton
               addTarget:self
                  action:@selector
                  (didTapIncognitoInterstitialDisabledInfoButton:)
        forControlEvents:UIControlEventTouchUpInside];
  }
  return cell;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (_settingsAreDismissed)
    return;

  [self enhancedSafeBrowsingInlinePromoTriggerCriteriaMet];

  if (preferenceName == prefs::kIosHandoffToOtherDevices) {
    NSString* detailText = _profile->GetPrefs()->GetBoolean(preferenceName)
                               ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                               : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _handoffDetailItem.detailText = detailText;
    [self reconfigureCellsForItems:@[ _handoffDetailItem ]];
    return;
  }

  DCHECK(_safeBrowsingDetailItem);
  if (preferenceName == prefs::kSafeBrowsingEnabled ||
      preferenceName == prefs::kSafeBrowsingEnhanced) {
    _safeBrowsingDetailItem.detailText = [self safeBrowsingDetailText];
    [self reconfigureCellsForItems:@[ _safeBrowsingDetailItem ]];
  }

  if (preferenceName == prefs::kBrowserLockdownModeEnabled) {
    NSString* detailText =
        GetApplicationContext()->GetLocalState()->GetBoolean(preferenceName)
            ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
            : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _lockdownModeDetailItem.detailText = detailText;
    [self reconfigureCellsForItems:@[ _lockdownModeDetailItem ]];
    return;
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  // Update the cells.
  self.incognitoReauthItem.on = self.incognitoReauthPref.value;
  [self reconfigureCellsForItems:@[ self.incognitoReauthItem ]];

  self.HTTPSOnlyModeItem.on = self.HTTPSOnlyModePref.value;
  [self reconfigureCellsForItems:@[ self.HTTPSOnlyModeItem ]];

  self.incognitoInterstitialItem.on = self.incognitoInterstitialPref.value;
  [self reconfigureCellsForItems:@[ self.incognitoInterstitialItem ]];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  if (URL.gurl == GURL(kGoogleServicesSettingsURL)) {
    // kGoogleServicesSettingsURL is not a realy link. It should be handled
    // with a special case.
    [self.settingsHandler showGoogleServicesSettingsFromViewController:self];
  } else if (URL.gurl == GURL(kSyncSettingsURL)) {
    [self.settingsHandler showSyncSettingsFromViewController:self];
  } else {
    [super view:view didTapLinkURL:URL];
  }
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [super view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updatePrivacyFooterItem];
}

#pragma mark - Private

// Called when the user taps on the information button of the disabled Incognito
// reauth setting's UI cell.
- (void)didTapIncognitoReauthDisabledInfoButton:(UIButton*)buttonView {
  InfoPopoverViewController* popover;
  if (supervised_user::IsSubjectToParentalControls(_profile)) {
    popover = [[SupervisedUserInfoPopoverViewController alloc]
        initWithMessage:
            l10n_util::GetNSString(
                IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_DISABLED_BY_PARENT)];
  } else if (IsIncognitoModeDisabled(_profile->GetPrefs())) {
    popover = [[EnterpriseInfoPopoverViewController alloc]
        initWithMessage:l10n_util::GetNSString(
                            IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_DISABLED)
         enterpriseName:nil];
  } else {
    popover = [[InfoPopoverViewController alloc]
        initWithMessage:l10n_util::GetNSString(
                            IDS_IOS_INCOGNITO_REAUTH_SET_UP_PASSCODE_HINT)];
  }

  [self showInfoPopover:popover forInfoButton:buttonView];
}

// Called when the user taps on the information button of the disabled Incognito
// interstitial setting's UI cell.
- (void)didTapIncognitoInterstitialDisabledInfoButton:(UIButton*)buttonView {
  InfoPopoverViewController* popover;
  if (supervised_user::IsSubjectToParentalControls(_profile)) {
    popover = [[SupervisedUserInfoPopoverViewController alloc]
        initWithMessage:
            l10n_util::GetNSString(
                IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_DISABLED_BY_PARENT)];
  } else {
    NSString* popoverMessage =
        IsIncognitoModeDisabled(_profile->GetPrefs())
            ? l10n_util::GetNSString(
                  IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_DISABLED)
            : l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_FORCED);

    popover = [[EnterpriseInfoPopoverViewController alloc]
        initWithMessage:popoverMessage
         enterpriseName:nil];
  }

  [self showInfoPopover:popover forInfoButton:buttonView];
}

// Shows a contextual bubble explaining that the tapped setting is managed and
// includes a link to the chrome://management page.
- (void)showInfoPopover:(PopoverLabelViewController*)popover
          forInfoButton:(UIButton*)buttonView {
  popover.delegate = self;

  // Disable the button when showing the bubble.
  // The button will be enabled when closing the bubble in
  // (void)popoverPresentationControllerDidDismissPopover: of
  // EnterpriseInfoPopoverViewController.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  popover.popoverPresentationController.sourceView = buttonView;
  popover.popoverPresentationController.sourceRect = buttonView.bounds;
  popover.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [self presentViewController:popover animated:YES completion:nil];
}

// Called from the HTTPS-Only Mode setting's UIControlEventTouchUpInside.
// When this is called, `switchView` already has the updated value:
// If the switch was off, and user taps it, when this method is called,
// switchView.on is YES.
- (void)HTTPSOnlyModeTapped:(UISwitch*)switchView {
  BOOL isOn = switchView.isOn;
  [_HTTPSOnlyModePref setValue:isOn];
  [self enhancedSafeBrowsingInlinePromoTriggerCriteriaMet];
}

// Called from the Incognito interstitial setting's UIControlEventTouchUpInside.
// When this is called, `switchView` already has the updated value:
// If the switch was off, and user taps it, when this method is called,
// switchView.on is YES.
- (void)incognitoInterstitialSwitchTapped:(UISwitch*)switchView {
  self.incognitoInterstitialPref.value = switchView.on;
  UMA_HISTOGRAM_ENUMERATION(
      kIncognitoInterstitialSettingsActionsHistogram,
      switchView.on ? IncognitoInterstitialSettingsActions::kEnabled
                    : IncognitoInterstitialSettingsActions::kDisabled);
  [self enhancedSafeBrowsingInlinePromoTriggerCriteriaMet];
}

// Called from the reauthentication setting's UIControlEventTouchUpInside.
// When this is called, `switchView` already has the updated value:
// If the switch was off, and user taps it, when this method is called,
// switchView.on is YES.
- (void)switchTapped:(UISwitch*)switchView {
  if (switchView.isOn && ![self.reauthModule canAttemptReauth]) {
    // This should normally not happen: the switch should not even be enabled.
    // Fallback behaviour added just in case.
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
                                 [weakSelf
                                     enhancedSafeBrowsingInlinePromoTriggerCriteriaMet];
                               }];
}

// Checks if the device has Passcode, Face ID, or Touch ID set up.
- (BOOL)deviceSupportsAuthentication {
  LAContext* context = [[LAContext alloc] init];
  return [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthentication
                              error:nil];
}

// Returns the proper detail text for the safe browsing item depending on the
// safe browsing and enhanced protection preference values.
- (NSString*)safeBrowsingDetailText {
  PrefService* prefService = _profile->GetPrefs();
  if (safe_browsing::IsEnhancedProtectionEnabled(*prefService)) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_TITLE);
  } else if (safe_browsing::IsSafeBrowsingEnabled(*prefService)) {
    return l10n_util::GetNSString(
        IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_TITLE);
  }
  return l10n_util::GetNSString(
      IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_DETAIL_TITLE);
}

- (void)enhancedSafeBrowsingInlinePromoTriggerCriteriaMet {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature) ||
      !_profile) {
    return;
  }
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(_profile);
  tracker->NotifyEvent(
      feature_engagement::events::kEnhancedSafeBrowsingPromoCriterionMet);
}

@end
