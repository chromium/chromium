// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"

#import <LocalAuthentication/LocalAuthentication.h>

#import "base/check.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/content_settings/core/common/features.h"
#import "components/handoff/pref_names_ios.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browsing_data/browsing_data_features.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_constants.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/elements/info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPrivacyContent = kSectionIdentifierEnumZero,
  SectionIdentifierSafeBrowsing,
  SectionIdentifierHTTPSOnlyMode,
  SectionIdentifierWebServices,
  SectionIdentifierIncognitoAuth,
  SectionIdentifierIncognitoInterstitial,
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
};

// Only used in this class to openn the Sync and Google services settings.
// This link should not be dispatched.
const char kGoogleServicesSettingsURL[] = "settings://open_google_services";
const char kSyncSettingsURL[] = "settings://open_sync";

}  // namespace

@interface PrivacyTableViewController () <BooleanObserver,
                                          PrefObserverDelegate,
                                          PopoverLabelViewControllerDelegate> {
  ChromeBrowserState* _browserState;  // weak

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Updatable Items.
  TableViewDetailIconItem* _handoffDetailItem;
  // Safe Browsing item.
  TableViewDetailIconItem* _safeBrowsingDetailItem;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
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
    _browserState = browser->GetBrowserState();
    self.title = l10n_util::GetNSString(IDS_IOS_SETTINGS_PRIVACY_TITLE);

    PrefService* prefService = _browserState->GetPrefs();

    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosHandoffToOtherDevices, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kSafeBrowsingEnabled, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kSafeBrowsingEnhanced, &_prefChangeRegistrar);

    _incognitoReauthPref = [[PrefBackedBoolean alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()
                   prefName:prefs::kIncognitoAuthenticationSetting];
    [_incognitoReauthPref setObserver:self];

    _HTTPSOnlyModePref = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::kHttpsOnlyModeEnabled];
    [_HTTPSOnlyModePref setObserver:self];

    _incognitoInterstitialPref = [[PrefBackedBoolean alloc]
        initWithPrefService:browser->GetBrowserState()->GetPrefs()
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

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed)
    return;

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierPrivacyContent];
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

  // Clear Browsing item.
  [model addItem:[self clearBrowsingDetailItem]
      toSectionWithIdentifier:SectionIdentifierPrivacyContent];

  // Privacy Safe Browsing item.
  [model addItem:[self safeBrowsingDetailItem]
      toSectionWithIdentifier:SectionIdentifierSafeBrowsing];
  [model setFooter:[self showPrivacyFooterItem]
      forSectionWithIdentifier:SectionIdentifierIncognitoInterstitial];

  // Web Services item.
  [model addItem:[self handoffDetailItem]
      toSectionWithIdentifier:SectionIdentifierWebServices];

  // Incognito reauth item is added. If Incognito mode is disabled, or device
  // authentication is not supported, a disabled version is shown instead with
  // relevant information as a popover.
  TableViewItem* incognitoReauthItem =
      (IsIncognitoModeDisabled(_browserState->GetPrefs()) ||
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
      (IsIncognitoModeDisabled(_browserState->GetPrefs()) ||
       IsIncognitoModeForced(_browserState->GetPrefs()))
          ? self.incognitoInterstitialItemDisabled
          : self.incognitoInterstitialItem;
  [model addItem:incognitoInterstitialItem
      toSectionWithIdentifier:SectionIdentifierIncognitoInterstitial];
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

  NSString* privacyFooterText;

  syncer::SyncService* syncService =
      SyncServiceFactory::GetInstance()->GetForBrowserState(_browserState);

  NSMutableArray* urls = [[NSMutableArray alloc] init];
  if (syncService->IsSyncFeatureEnabled()) {
    privacyFooterText =
        l10n_util::GetNSString(IDS_IOS_PRIVACY_SYNC_AND_GOOGLE_SERVICES_FOOTER);
    [urls addObject:[[CrURL alloc] initWithGURL:GURL(kSyncSettingsURL)]];
  } else {
    privacyFooterText =
        l10n_util::GetNSString(IDS_IOS_PRIVACY_GOOGLE_SERVICES_FOOTER);
  }
  [urls
      addObject:[[CrURL alloc] initWithGURL:GURL(kGoogleServicesSettingsURL)]];

  showPrivacyFooterItem.text = privacyFooterText;
  showPrivacyFooterItem.urls = urls;
  return showPrivacyFooterItem;
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

  // Remove observer bridges.
  _prefObserverBridge.reset();

  // Clear C++ ivars.
  _browserState = nullptr;

  _settingsAreDismissed = YES;
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
    case ItemTypePrivacySafeBrowsing:
      base::RecordAction(base::UserMetricsAction(
          "SafeBrowsing.Settings.ShowedFromParentSettings"));
      [self.handler showSafeBrowsing];
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
        base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchTapped:)
                    forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeIncognitoReauthDisabled) {
    TableViewInfoButtonCell* managedCell =
        base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
    [managedCell.trailingButton
               addTarget:self
                  action:@selector(didTapIncognitoReauthDisabledInfoButton:)
        forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeHTTPSOnlyMode) {
    TableViewSwitchCell* switchCell =
        base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(HTTPSOnlyModeTapped:)
                    forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeIncognitoInterstitial) {
    TableViewSwitchCell* switchCell =
        base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView
               addTarget:self
                  action:@selector(incognitoInterstitialSwitchTapped:)
        forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeIncognitoInterstitialDisabled) {
    TableViewInfoButtonCell* managedCell =
        base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
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

  if (preferenceName == prefs::kIosHandoffToOtherDevices) {
    NSString* detailText = _browserState->GetPrefs()->GetBoolean(preferenceName)
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
    [self.dispatcher showGoogleServicesSettingsFromViewController:self];
  } else if (URL.gurl == GURL(kSyncSettingsURL)) {
    [self.dispatcher showSyncSettingsFromViewController:self];
  } else {
    [super view:view didTapLinkURL:URL];
  }
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [super view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

#pragma mark - Private

// Called when the user taps on the information button of the disabled Incognito
// reauth setting's UI cell.
- (void)didTapIncognitoReauthDisabledInfoButton:(UIButton*)buttonView {
  NSString* popoverMessage =
      IsIncognitoModeDisabled(_browserState->GetPrefs())
          ? l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_DISABLED)
          : l10n_util::GetNSString(
                IDS_IOS_INCOGNITO_REAUTH_SET_UP_PASSCODE_HINT);
  InfoPopoverViewController* popover =
      IsIncognitoModeDisabled(_browserState->GetPrefs())
          ? [[EnterpriseInfoPopoverViewController alloc]
                initWithMessage:popoverMessage
                 enterpriseName:nil]
          : [[InfoPopoverViewController alloc] initWithMessage:popoverMessage];

  [self showInfoPopover:popover forInfoButton:buttonView];
}

// Called when the user taps on the information button of the disabled Incognito
// interstitial setting's UI cell.
- (void)didTapIncognitoInterstitialDisabledInfoButton:(UIButton*)buttonView {
  NSString* popoverMessage =
      IsIncognitoModeDisabled(_browserState->GetPrefs())
          ? l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_DISABLED)
          : l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_FORCED);
  EnterpriseInfoPopoverViewController* popover =
      [[EnterpriseInfoPopoverViewController alloc]
          initWithMessage:popoverMessage
           enterpriseName:nil];

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
  PrefService* prefService = _browserState->GetPrefs();
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

@end
