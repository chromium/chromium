// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/plus_addresses/features.h"
#import "components/plus_addresses/grit/plus_addresses_strings.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/util.h"
#import "components/send_tab_to_self/features.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/language/model/language_model_manager_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/photos/model/photos_service.h"
#import "ios/chrome/browser/photos/model/photos_service_factory.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_state.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/settings/about_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/address_bar_preference/address_bar_preference_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/bandwidth/bandwidth_management_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"
#import "ios/chrome/browser/ui/settings/cells/enhanced_safe_browsing_inline_promo_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/content_settings/content_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/default_browser/default_browser_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_coordinator.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/enhanced_safe_browsing_inline_promo_delegate.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_coordinator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_utils.h"
#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/table_cell_catalog_view_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/voice_search_table_view_controller.h"
#import "ios/chrome/browser/upgrade/model/upgrade_utils.h"
#import "ios/chrome/browser/voice/model/speech_input_locale_config.h"
#import "ios/chrome/browser/voice/model/voice_search_prefs.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The maximum number of time a "new" IPH badge is shown.
const NSInteger kMaxShowCountNewIPHBadge = 3;
// The amount of time an install is considered as fresh. Don't show the "new"
// IPH badge on fresh installs.
const base::TimeDelta kFreshInstallTimeDelta = base::Days(1);

#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
NSString* kDevViewSourceKey = @"DevViewSource";
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return CustomSettingsRootMulticolorSymbol(kGoogleIconSymbol);
#else
  return DefaultSettingsRootSymbol(@"gearshape.2");
#endif
}

// Struct used to count and store the number of active Enhanced Safe Browsing
// promos, as the FET does not support showing multiple badges for the same FET
// feature at the same time.
struct EnhancedSafeBrowsingActivePromoData
    : public base::SupportsUserData::Data {
  // The number of active menus.
  int active_promos = 0;

  // Key to use for this type in SupportsUserData
  static constexpr char key[] = "EnhancedSafeBrowsingActivePromoData";
};

}  // namespace

#pragma mark - SettingsTableViewController

@interface SettingsTableViewController () <
    BooleanObserver,
    ChromeAccountManagerServiceObserver,
    DownloadsSettingsCoordinatorDelegate,
    EnhancedSafeBrowsingInlinePromoDelegate,
    GoogleServicesSettingsCoordinatorDelegate,
    IdentityManagerObserverBridgeDelegate,
    ManageSyncSettingsCoordinatorDelegate,
    NotificationsSettingsObserverDelegate,
    PasswordCheckObserver,
    PasswordsCoordinatorDelegate,
    PopoverLabelViewControllerDelegate,
    PrefObserverDelegate,
    NotificationsCoordinatorDelegate,
    PrivacyCoordinatorDelegate,
    SafetyCheckCoordinatorDelegate,
    SearchEngineObserving,
    SyncObserverModelBridge> {
  // The browser where the settings are being displayed.
  raw_ptr<Browser> _browser;
  // The profile for `_browser`. Never off the record.
  raw_ptr<ProfileIOS> _profile;  // weak
  // Bridge for TemplateURLServiceObserver.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserverBridge;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  // A helper object for observing changes in the password check status
  // and changes to the compromised credentials list.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;
  // The service responsible for password check feature.
  scoped_refptr<IOSChromePasswordCheckManager> _passwordCheckManager;
  // Whether the impression of the Signin button has already been recorded.
  BOOL _hasRecordedSigninImpression;
  // PrefBackedBoolean for ShowMemoryDebugTools switch.
  PrefBackedBoolean* _showMemoryDebugToolsEnabled;
  // PrefBackedBoolean for ArticlesForYou switch.
  PrefBackedBoolean* _articlesEnabled;
  // Preference value for the "Allow Chrome Sign-in" feature.
  PrefBackedBoolean* _allowChromeSigninPreference;
  // PrefBackedBoolean for ArticlesForYou switch enabling.
  PrefBackedBoolean* _contentSuggestionPolicyEnabled;
  // PrefBackedBoolean that overrides ArticlesForYou switch for supervised
  // users.
  PrefBackedBoolean* _contentSuggestionForSupervisedUsersEnabled;
  // PrefBackedBoolean for BottomOmnibox switch.
  PrefBackedBoolean* _bottomOmniboxEnabled;
  // The item related to the switch for the show suggestions setting.
  TableViewSwitchItem* _showMemoryDebugToolsItem;
  // The item related to the safety check.
  SettingsCheckItem* _safetyCheckItem;

  GoogleServicesSettingsCoordinator* _googleServicesSettingsCoordinator;
  ManageSyncSettingsCoordinator* _manageSyncSettingsCoordinator;

  // notifications coordinator.
  NotificationsCoordinator* _notificationsCoordinator;

  // Privacy coordinator.
  PrivacyCoordinator* _privacyCoordinator;

  // Safety Check coordinator.
  SafetyCheckCoordinator* _safetyCheckCoordinator;

  // Passwords coordinator.
  PasswordsCoordinator* _passwordsCoordinator;

  // Accounts coordinator.
  AccountsCoordinator* _accountsCoordinator;

  // Feature engagement tracker for the signin IPH.
  raw_ptr<feature_engagement::Tracker> _featureEngagementTracker;
  // Presenter for the signin IPH.
  BubbleViewControllerPresenter* _bubblePresenter;

  // Identity object and observer used for Account Item refresh.
  id<SystemIdentity> _identity;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;

  // PrefMember for voice locale code.
  StringPrefMember _voiceLocaleCode;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // TODO(crbug.com/40492152): Refactor PrefObserverBridge so it owns the
  // PrefChangeRegistrar.
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Updatable Items.
  TableViewDetailIconItem* _voiceSearchDetailItem;
  TableViewDetailIconItem* _defaultSearchEngineItem;
  TableViewInfoButtonItem* _managedSearchEngineItem;
  TableViewDetailIconItem* _addressBarPreferenceItem;
  TableViewDetailIconItem* _passwordsDetailItem;
  TableViewDetailIconItem* _autoFillProfileDetailItem;
  TableViewDetailIconItem* _autoFillCreditCardDetailItem;
  TableViewDetailIconItem* _notificationsItem;
  TableViewDetailIconItem* _plusAddressesItem;
  TableViewDetailIconItem* _defaultBrowserCellItem;
  TableViewItem* _syncItem;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // Tabs settings coordinator.
  TabsSettingsCoordinator* _tabsCoordinator;

  // Switch profile coordinator.
  SwitchProfileSettingsCoordinator* _switchProfileCoordinator;

  // Address bar setting coordinator.
  AddressBarPreferenceCoordinator* _addressBarPreferenceCoordinator;

  // Downloads settings coordinator.
  DownloadsSettingsCoordinator* _downloadsSettingsCoordinator;
}

// The item related to the switch for the show feed settings.
@property(nonatomic, strong, readonly) TableViewSwitchItem* feedSettingsItem;
// The item related to the enterprise managed show feed settings.
@property(nonatomic, strong, readonly)
    TableViewInfoButtonItem* managedFeedSettingsItem;

// YES if the default browser settings row is currently showing the notification
// dot.
@property(nonatomic, assign) BOOL showingDefaultBrowserNotificationDot;

// YES if the sign-in is in progress.
@property(nonatomic, assign) BOOL isSigninInProgress;

// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

// An observer that tracks whether push notification permission settings have
// been modified.
@property(nonatomic, strong)
    NotificationsSettingsObserver* notificationsObserver;

@end

@implementation SettingsTableViewController
@synthesize managedFeedSettingsItem = _managedFeedSettingsItem;
@synthesize feedSettingsItem = _feedSettingsItem;

#pragma mark Initialization

- (instancetype)initWithBrowser:(Browser*)browser
       hasDefaultBrowserBlueDot:(BOOL)hasDefaultBrowserBlueDot {
  DCHECK(browser);
  DCHECK(!browser->GetProfile()->IsOffTheRecord());

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    _profile = _browser->GetProfile();
    self.showingDefaultBrowserNotificationDot = hasDefaultBrowserBlueDot;
    self.title = l10n_util::GetNSStringWithFixup(IDS_IOS_SETTINGS_TITLE);
    _searchEngineObserverBridge.reset(new SearchEngineObserverBridge(
        self, ios::TemplateURLServiceFactory::GetForProfile(_profile)));
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(_profile);
    _accountManagerService =
        ChromeAccountManagerServiceFactory::GetForProfile(_profile);
    // It is expected that `identityManager` should never be nil except in
    // tests. In that case, the tests should be fixed.
    DCHECK(identityManager);
    _identityObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(identityManager, self));
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForProfile(_profile);
    _syncObserverBridge.reset(new SyncObserverBridge(self, syncService));

    PrefService* localState = GetApplicationContext()->GetLocalState();
    _showMemoryDebugToolsEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:localState
                   prefName:prefs::kShowMemoryDebuggingTools];
    [_showMemoryDebugToolsEnabled setObserver:self];

    AuthenticationService* authService =
        AuthenticationServiceFactory::GetForProfile(_profile);
    _identity = authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
    _accountManagerServiceObserver.reset(
        new ChromeAccountManagerServiceObserverBridge(self,
                                                      _accountManagerService));
    _featureEngagementTracker =
        feature_engagement::TrackerFactory::GetForProfile(_profile);

    PrefService* prefService = _profile->GetPrefs();

    _passwordCheckManager =
        IOSChromePasswordCheckManagerFactory::GetForProfile(_profile);
    _passwordCheckObserver = std::make_unique<PasswordCheckObserverBridge>(
        self, _passwordCheckManager.get());

    _allowChromeSigninPreference =
        [[PrefBackedBoolean alloc] initWithPrefService:prefService
                                              prefName:prefs::kSigninAllowed];
    _allowChromeSigninPreference.observer = self;

    _articlesEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::kArticlesForYouEnabled];
    [_articlesEnabled setObserver:self];

    _bottomOmniboxEnabled =
        [[PrefBackedBoolean alloc] initWithPrefService:localState
                                              prefName:prefs::kBottomOmnibox];
    [_bottomOmniboxEnabled setObserver:self];

    _contentSuggestionPolicyEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::kNTPContentSuggestionsEnabled];
    [_contentSuggestionPolicyEnabled setObserver:self];

    _contentSuggestionForSupervisedUsersEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::
                                kNTPContentSuggestionsForSupervisedUserEnabled];
    [_contentSuggestionForSupervisedUsersEnabled setObserver:self];

    _voiceLocaleCode.Init(prefs::kVoiceSearchLocale, prefService);

    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(prefs::kVoiceSearchLocale,
                                                     &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        password_manager::prefs::kCredentialsEnableService,
        &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillCreditCardEnabled, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillProfileEnabled, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName,
        &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(prefs::kSigninAllowed,
                                                     &_prefChangeRegistrar);
    _notificationsObserver =
        [[NotificationsSettingsObserver alloc] initWithPrefService:prefService
                                                        localState:localState];
    _notificationsObserver.delegate = self;

    // TODO(crbug.com/41344225): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

- (void)dealloc {
  DCHECK(_settingsAreDismissed)
      << "-settingsWillBeDismissed must be called before -dealloc";
}

#pragma mark View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier = kSettingsTableViewId;

  // Change the separator inset from the settings default because this
  // TableView shows leading icons.
  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorInsetWithIcon, 0, 0);

  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeAlways;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Update the `_safetyCheckItem` icon when returning to this view controller.
  [self updateSafetyCheckItemTrailingIcon];
  if (IsBottomOmniboxAvailable()) {
    // Update the address bar new IPH badge here as it depends on the number of
    // time it's shown.
    [self updateAddressBarNewIPHBadge];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self maybeShowSigninIPH];
}

#pragma mark SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];

  // Sign-in section.
  [self updateSigninSection];

  // Defaults section.
  TableViewModel<TableViewItem*>* model = self.tableViewModel;
  [model addSectionWithIdentifier:SettingsSectionIdentifierDefaults];
  [model addItem:[self defaultBrowserCellItem]
      toSectionWithIdentifier:SettingsSectionIdentifierDefaults];

  // Show managed UI if default search engine is managed by policy.
  if ([self isDefaultSearchEngineManagedByPolicy]) {
    [model addItem:[self managedSearchEngineItem]
        toSectionWithIdentifier:SettingsSectionIdentifierDefaults];
  } else {
    [model addItem:[self searchEngineDetailItem]
        toSectionWithIdentifier:SettingsSectionIdentifierDefaults];
  }

  if (IsBottomOmniboxAvailable()) {
    [model addItem:[self addressBarPreferenceItem]
        toSectionWithIdentifier:SettingsSectionIdentifierDefaults];
  }

  // Basics section
  [model addSectionWithIdentifier:SettingsSectionIdentifierBasics];
  [model addItem:[self passwordsDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];
  [model addItem:[self autoFillCreditCardDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];
  [model addItem:[self autoFillProfileDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];
  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled)) {
    _plusAddressesItem = [self plusAddressesItem];
    [model addItem:_plusAddressesItem
        toSectionWithIdentifier:SettingsSectionIdentifierBasics];
  }

  // Advanced Section
  [model addSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
  if ([self shouldShowNotificationsSettings]) {
    _notificationsItem = [self notificationsItem];
    [self updateNotificationsDetailText];
    [model addItem:_notificationsItem
        toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
  }
  [model addItem:[self voiceSearchDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
  [model addItem:[self safetyCheckDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
  [model addItem:[self privacyDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];

  // Feed is disabled in safe mode.
  SceneState* sceneState = _browser->GetSceneState();
  BOOL isSafeMode = [sceneState.appState resumingFromSafeMode];
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(_profile);

  if (!IsFeedAblationEnabled() && !isSafeMode &&
      IsContentSuggestionsForSupervisedUserEnabled(_profile->GetPrefs()) &&
      !ShouldHideFeedWithSearchChoice(templateURLService)) {
    if ([_contentSuggestionPolicyEnabled value]) {
      [model addItem:self.feedSettingsItem
          toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];

    } else {
      [model addItem:self.managedFeedSettingsItem
          toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
    }
  }

  PhotosService* photosService = PhotosServiceFactory::GetForProfile(_profile);
  bool shouldShowDownloadsSettings =
      photosService && photosService->IsSupported();
  if (IsInactiveTabsAvailable()) {
    [model addItem:[self tabsSettingsDetailItem]
        toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];

    // Info Section
    [model addSectionWithIdentifier:SettingsSectionIdentifierInfo];
    [model addItem:[self languageSettingsDetailItem]
        toSectionWithIdentifier:SettingsSectionIdentifierInfo];
    [model addItem:[self contentSettingsDetailItem]
        toSectionWithIdentifier:SettingsSectionIdentifierInfo];
    if (shouldShowDownloadsSettings) {
      [model addItem:[self downloadsSettingsDetailItem]
          toSectionWithIdentifier:SettingsSectionIdentifierInfo];
    }
    [model addItem:[self bandwidthManagementDetailItem]
        toSectionWithIdentifier:SettingsSectionIdentifierInfo];
  } else {
    [model addItem:[self languageSettingsDetailItem]
        toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
    [model addItem:[self contentSettingsDetailItem]
        toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
    if (shouldShowDownloadsSettings) {
      [model addItem:[self downloadsSettingsDetailItem]
          toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
    }
    [model addItem:[self bandwidthManagementDetailItem]
        toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];

    // Info Section
    [model addSectionWithIdentifier:SettingsSectionIdentifierInfo];
  }
  [model addItem:[self aboutChromeDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierInfo];

  // Debug Section
  if ([self hasDebugSection]) {
    [model addSectionWithIdentifier:SettingsSectionIdentifierDebug];
  }

  if (experimental_flags::IsMemoryDebuggingEnabled()) {
    _showMemoryDebugToolsItem = [self showMemoryDebugSwitchItem];
    [model addItem:_showMemoryDebugToolsItem
        toSectionWithIdentifier:SettingsSectionIdentifierDebug];

    if (experimental_flags::DisplaySwitchProfile().has_value()) {
      [model addItem:[self switchProfileItem]
          toSectionWithIdentifier:SettingsSectionIdentifierDebug];
    }
  }

#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
  [model addItem:[self viewSourceSwitchItem]
      toSectionWithIdentifier:SettingsSectionIdentifierDebug];
  [model addItem:[self tableViewCatalogDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierDebug];
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
}

- (void)updateSigninSection {
  TableViewModel<TableViewItem*>* model = self.tableViewModel;
  if ([model hasSectionForSectionIdentifier:SettingsSectionIdentifierSignIn]) {
    [model removeSectionWithIdentifier:SettingsSectionIdentifierSignIn];
  }
  if ([model hasSectionForSectionIdentifier:SettingsSectionIdentifierAccount]) {
    [model removeSectionWithIdentifier:SettingsSectionIdentifierAccount];
  }

  [model insertSectionWithIdentifier:SettingsSectionIdentifierAccount
                             atIndex:0];
  [self addAccountToSigninSection];

  // Temporarily place this in the first index position in case it is populated.
  // If this is not the case SettingsSectionIdentifierAccount will remain at
  // index 0.
  [model insertSectionWithIdentifier:SettingsSectionIdentifierSignIn atIndex:0];
  [self addPromoToSigninSection];
  [self addPromoToEnhancedSafeBrowsingSection];
}

// Adds the identity promo to promote the sign-in or sync state.
- (void)addPromoToSigninSection {
  TableViewItem* item = nil;

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(_profile);
  const AuthenticationService::ServiceStatus authServiceStatus =
      authService->GetServiceStatus();
  // If sign-in is disabled by policy there should not be a sign-in promo.
  if ((authServiceStatus ==
       AuthenticationService::ServiceStatus::SigninDisabledByPolicy)) {
    item = [self signinDisabledByPolicyTextItem];
  } else if ((authServiceStatus ==
                  AuthenticationService::ServiceStatus::SigninForcedByPolicy ||
              authServiceStatus ==
                  AuthenticationService::ServiceStatus::SigninAllowed) &&
             !authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    item = [self accountSignInItem];
  } else {
    [self.tableViewModel
        removeSectionWithIdentifier:SettingsSectionIdentifierSignIn];

    if (!_hasRecordedSigninImpression) {
      // Once the Settings are open, this button impression will at most be
      // recorded once until they are closed.
      signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
          signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
      _hasRecordedSigninImpression = YES;
    }

    // The user is signed-in and syncing, exit early since the promo is not
    // required.
    return;
  }

  [self.tableViewModel addItem:item
       toSectionWithIdentifier:SettingsSectionIdentifierSignIn];
}

// Adds the account profile to the Account section if the user is signed in.
- (void)addAccountToSigninSection {
  TableViewModel<TableViewItem*>* model = self.tableViewModel;
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(_profile);
  if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    // Account profile item.
    [model addItem:[self accountCellItem]
        toSectionWithIdentifier:SettingsSectionIdentifierAccount];
    _hasRecordedSigninImpression = NO;
  }

  // Sync item.
  if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin) &&
      ![self shouldReplaceSyncSettingsWithAccountSettings]) {
    [model addItem:[self syncItem]
        toSectionWithIdentifier:SettingsSectionIdentifierAccount];
  }
  // Google Services item.
  [model addItem:[self googleServicesCellItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAccount];
}

// Adds the Enhanced Safe Browsing inline promo to promote ESB.
- (void)addPromoToEnhancedSafeBrowsingSection {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature) ||
      ![self shouldShowEnhancedSafeBrowsingPromo]) {
    return;
  }

  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SettingsSectionIdentifierESBPromo]) {
    [self.tableViewModel
        removeSectionWithIdentifier:SettingsSectionIdentifierESBPromo];
  }
  [self.tableViewModel
      insertSectionWithIdentifier:SettingsSectionIdentifierESBPromo
                          atIndex:0];

  if (![self.tableViewModel
          hasItemForItemType:SettingsItemTypeESBPromo
           sectionIdentifier:SettingsSectionIdentifierESBPromo]) {
    [self.tableViewModel addItem:[self enhancedSafeBrowsingInlinePromoItem]
         toSectionWithIdentifier:SettingsSectionIdentifierESBPromo];
  }

  [self maybeRecordEnhancedSafeBrowsingImpressionLimitReached];
}

#pragma mark - Model Items

- (TableViewItem*)accountSignInItem {
  AccountSignInItem* signInTextItem =
      [[AccountSignInItem alloc] initWithType:SettingsItemTypeSignInButton];
  signInTextItem.accessibilityIdentifier = kSettingsSignInCellId;
  // TODO(crbug.com/40064662): Make detailText private when the feature is
  // launched.
  signInTextItem.detailText =
      l10n_util::GetNSString(IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL);
  return signInTextItem;
}

- (TableViewItem*)signinDisabledByPolicyTextItem {
  TableViewInfoButtonItem* signinDisabledItem =
      [self infoButtonWithType:SettingsItemTypeSigninDisabled
                             text:l10n_util::GetNSString(
                                      IDS_IOS_SIGN_IN_TO_CHROME_SETTING_TITLE)
                           status:l10n_util::GetNSString(IDS_IOS_SETTING_OFF)
                            image:nil
                  imageBackground:nil
                accessibilityHint:
                    l10n_util::GetNSString(
                        IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT)
          accessibilityIdentifier:kSettingsSignInDisabledCellId];
  signinDisabledItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return signinDisabledItem;
}

- (TableViewItem*)googleServicesCellItem {
  return [self detailItemWithType:SettingsItemTypeGoogleServices
                             text:l10n_util::GetNSString(
                                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE)
                       detailText:nil
                           symbol:GetBrandedGoogleServicesSymbol()
            symbolBackgroundColor:nil
          accessibilityIdentifier:kSettingsGoogleServicesCellId];
}

- (TableViewItem*)syncDisabledByPolicyItem {
  return [self infoButtonWithType:SettingsItemTypeGoogleSync
                             text:l10n_util::GetNSString(
                                      IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE)
                           status:l10n_util::GetNSString(IDS_IOS_SETTING_OFF)
                            image:CustomSettingsRootSymbol(kSyncDisabledSymbol)
                  imageBackground:[UIColor colorNamed:kGrey400Color]
                accessibilityHint:
                    l10n_util::GetNSString(
                        IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT)
          accessibilityIdentifier:kSettingsGoogleSyncAndServicesCellId];
}

- (TableViewItem*)syncItem {
  if ([self isSyncDisabledByPolicy]) {
    _syncItem = [self syncDisabledByPolicyItem];
    return _syncItem;
  }

  TableViewDetailIconItem* syncItem =
      [self detailItemWithType:SettingsItemTypeGoogleSync
                             text:l10n_util::GetNSString(
                                      IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE)
                       detailText:nil
                           symbol:nil
            symbolBackgroundColor:nil
          accessibilityIdentifier:kSettingsGoogleSyncAndServicesCellId];
  [self updateSyncItem:syncItem];
  _syncItem = syncItem;

  return _syncItem;
}

- (TableViewItem*)defaultBrowserCellItem {
  _defaultBrowserCellItem = [[TableViewDetailIconItem alloc]
      initWithType:SettingsItemTypeDefaultBrowser];
  _defaultBrowserCellItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  _defaultBrowserCellItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_DEFAULT_BROWSER);

  _defaultBrowserCellItem.iconImage =
      DefaultSettingsRootSymbol(kDefaultBrowserSymbol);
  _defaultBrowserCellItem.iconBackgroundColor =
      [UIColor colorNamed:kPurple500Color];
  _defaultBrowserCellItem.iconTintColor = UIColor.whiteColor;
  _defaultBrowserCellItem.iconCornerRadius =
      kColorfulBackgroundSymbolCornerRadius;

  [self updateDefaultBrowserSettingsBlueDot];

  return _defaultBrowserCellItem;
}

- (TableViewItem*)accountCellItem {
  TableViewAccountItem* identityAccountItem =
      [[TableViewAccountItem alloc] initWithType:SettingsItemTypeAccount];
  identityAccountItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  identityAccountItem.accessibilityIdentifier = kSettingsAccountCellId;
  [self updateIdentityAccountItem:identityAccountItem];
  return identityAccountItem;
}

- (TableViewItem*)searchEngineDetailItem {
  NSString* defaultSearchEngineName =
      base::SysUTF16ToNSString(GetDefaultSearchEngineName(
          ios::TemplateURLServiceFactory::GetForProfile(_profile)));

  _defaultSearchEngineItem =
      [self detailItemWithType:SettingsItemTypeSearchEngine
                             text:l10n_util::GetNSString(
                                      IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)
                       detailText:defaultSearchEngineName
                           symbol:DefaultSettingsRootSymbol(kSearchSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kPurple500Color]
          accessibilityIdentifier:kSettingsSearchEngineCellId];

  return _defaultSearchEngineItem;
}

- (TableViewItem*)addressBarPreferenceItem {
  _addressBarPreferenceItem = [self
           detailItemWithType:SettingsItemTypeAddressBar
                         text:l10n_util::GetNSString(
                                  IDS_IOS_ADDRESS_BAR_SETTING)
                   detailText:[_bottomOmniboxEnabled value]
                                  ? l10n_util::GetNSString(
                                        IDS_IOS_BOTTOM_ADDRESS_BAR_OPTION)
                                  : l10n_util::GetNSString(
                                        IDS_IOS_TOP_ADDRESS_BAR_OPTION)
                       symbol:DefaultSettingsRootSymbol(kGlobeAmericasSymbol)
        symbolBackgroundColor:[UIColor colorNamed:kPurple500Color]
      accessibilityIdentifier:kSettingsAddressBarCellId];
  return _addressBarPreferenceItem;
}

- (TableViewInfoButtonItem*)managedSearchEngineItem {
  _managedSearchEngineItem =
      [self infoButtonWithType:SettingsItemTypeManagedDefaultSearchEngine
                             text:l10n_util::GetNSString(
                                      IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)
                           status:[self managedSearchEngineDetailText]
                            image:DefaultSettingsRootSymbol(kSearchSymbol)
                  imageBackground:[UIColor colorNamed:kPurple500Color]
                accessibilityHint:
                    l10n_util::GetNSString(
                        IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT)
          accessibilityIdentifier:kSettingsManagedSearchEngineCellId];

  return _managedSearchEngineItem;
}

- (TableViewItem*)passwordsDetailItem {
  BOOL passwordsEnabled = _profile->GetPrefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService);

  NSString* passwordsDetail = passwordsEnabled
                                  ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

  NSString* passwordsSectionTitle =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);

  _passwordsDetailItem =
      [self detailItemWithType:SettingsItemTypePasswords
                             text:passwordsSectionTitle
                       detailText:passwordsDetail
                           symbol:CustomSettingsRootSymbol(kPasswordSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kYellow500Color]
          accessibilityIdentifier:kSettingsPasswordsCellId];

  return _passwordsDetailItem;
}

- (TableViewItem*)autoFillCreditCardDetailItem {
  BOOL autofillCreditCardEnabled =
      autofill::prefs::IsAutofillPaymentMethodsEnabled(_profile->GetPrefs());
  NSString* detailText = autofillCreditCardEnabled
                             ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                             : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

  _autoFillCreditCardDetailItem =
      [self detailItemWithType:SettingsItemTypeAutofillCreditCard
                             text:l10n_util::GetNSString(
                                      IDS_AUTOFILL_PAYMENT_METHODS)
                       detailText:detailText
                           symbol:DefaultSettingsRootSymbol(kCreditCardSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kYellow500Color]
          accessibilityIdentifier:kSettingsPaymentMethodsCellId];

  return _autoFillCreditCardDetailItem;
}

- (TableViewItem*)autoFillProfileDetailItem {
  BOOL autofillProfileEnabled =
      autofill::prefs::IsAutofillProfileEnabled(_profile->GetPrefs());
  NSString* detailText = autofillProfileEnabled
                             ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                             : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

  _autoFillProfileDetailItem =
      [self detailItemWithType:SettingsItemTypeAutofillProfile
                             text:l10n_util::GetNSString(
                                      IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE)
                       detailText:detailText
                           symbol:CustomSettingsRootSymbol(kLocationSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kYellow500Color]
          accessibilityIdentifier:kSettingsAddressesAndMoreCellId];
  return _autoFillProfileDetailItem;
}

- (TableViewItem*)voiceSearchDetailItem {
  voice::SpeechInputLocaleConfig* localeConfig =
      voice::SpeechInputLocaleConfig::GetInstance();
  voice::SpeechInputLocale locale =
      _voiceLocaleCode.GetValue().length()
          ? localeConfig->GetLocaleForCode(_voiceLocaleCode.GetValue())
          : localeConfig->GetDefaultLocale();
  NSString* languageName = base::SysUTF16ToNSString(locale.display_name);

  _voiceSearchDetailItem =
      [self detailItemWithType:SettingsItemTypeVoiceSearch
                             text:l10n_util::GetNSString(
                                      IDS_IOS_VOICE_SEARCH_SETTING_TITLE)
                       detailText:languageName
                           symbol:DefaultSettingsRootSymbol(kMicrophoneSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kGreen500Color]
          accessibilityIdentifier:kSettingsVoiceSearchCellId];

  return _voiceSearchDetailItem;
}

- (SettingsCheckItem*)safetyCheckDetailItem {
  NSString* safetyCheckTitle =
      l10n_util::GetNSString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_SAFETY_CHECK);
  _safetyCheckItem =
      [[SettingsCheckItem alloc] initWithType:SettingsItemTypeSafetyCheck];
  _safetyCheckItem.text = safetyCheckTitle;
  _safetyCheckItem.enabled = YES;
  _safetyCheckItem.indicatorHidden = YES;
  _safetyCheckItem.infoButtonHidden = YES;
  _safetyCheckItem.trailingImage = nil;
  _safetyCheckItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  _safetyCheckItem.leadingIcon = CustomSettingsRootSymbol(kSafetyCheckSymbol);
  _safetyCheckItem.leadingIconBackgroundColor =
      [UIColor colorNamed:kBlue500Color];
  _safetyCheckItem.leadingIconTintColor = UIColor.whiteColor;
  _safetyCheckItem.leadingIconCornerRadius =
      kColorfulBackgroundSymbolCornerRadius;
  _safetyCheckItem.accessibilityIdentifier = kSettingsSafetyCheckCellId;
  // Check if an issue state should be shown for updates.
  if (!IsAppUpToDate() && PreviousSafetyCheckIssueFound()) {
    [self updateSafetyCheckItemTrailingIcon];
  }

  return _safetyCheckItem;
}

- (TableViewDetailIconItem*)notificationsItem {
  NSString* title = l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_TITLE);
  return [self detailItemWithType:SettingsItemTypeNotifications
                             text:title
                       detailText:nil
                           symbol:DefaultSettingsRootSymbol(kBellSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kPink500Color]
          accessibilityIdentifier:kSettingsNotificationsId];
}

- (TableViewDetailIconItem*)plusAddressesItem {
  NSString* title = l10n_util::GetNSString(IDS_PLUS_ADDRESS_SETTINGS_LABEL);

  return [self
           detailItemWithType:SettingsItemTypePlusAddresses
                         text:title
                   detailText:nil
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
                       symbol:CustomSettingsRootSymbol(kGooglePlusAddressSymbol)
#else
                       symbol:nil
#endif
        symbolBackgroundColor:[UIColor colorNamed:kYellow500Color]
      accessibilityIdentifier:kSettingsPlusAddressesId];
}

- (TableViewItem*)privacyDetailItem {
  NSString* title = nil;
  title = l10n_util::GetNSString(IDS_IOS_SETTINGS_PRIVACY_TITLE);

  return [self detailItemWithType:SettingsItemTypePrivacy
                             text:title
                       detailText:nil
                           symbol:CustomSettingsRootSymbol(kPrivacySymbol)
            symbolBackgroundColor:[UIColor colorNamed:kBlue500Color]
          accessibilityIdentifier:kSettingsPrivacyCellId];
}

- (TableViewSwitchItem*)feedSettingsItem {
  if (!_feedSettingsItem) {
    NSString* settingTitle = [self feedItemTitle];

    _feedSettingsItem =
        [self switchItemWithType:SettingsItemTypeArticlesForYou
                              title:settingTitle
                             symbol:DefaultSettingsRootSymbol(kDiscoverSymbol)
              symbolBackgroundColor:[UIColor colorNamed:kOrange500Color]
            accessibilityIdentifier:kSettingsArticleSuggestionsCellId];
    _feedSettingsItem.on = [_articlesEnabled value];
  }
  return _feedSettingsItem;
}

- (TableViewInfoButtonItem*)managedFeedSettingsItem {
  if (!_managedFeedSettingsItem) {
    _managedFeedSettingsItem =
        [self infoButtonWithType:SettingsItemTypeManagedArticlesForYou
                               text:[self feedItemTitle]
                             status:l10n_util::GetNSString(IDS_IOS_SETTING_OFF)
                              image:DefaultSettingsRootSymbol(kDiscoverSymbol)
                    imageBackground:[UIColor colorNamed:kOrange500Color]
                  accessibilityHint:
                      l10n_util::GetNSString(
                          IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT)
            accessibilityIdentifier:kSettingsArticleSuggestionsCellId];
  }

  return _managedFeedSettingsItem;
}

- (TableViewItem*)languageSettingsDetailItem {
  return [self detailItemWithType:SettingsItemTypeLanguageSettings
                             text:l10n_util::GetNSString(
                                      IDS_IOS_LANGUAGE_SETTINGS_TITLE)
                       detailText:nil
                           symbol:CustomSettingsRootSymbol(kLanguageSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
          accessibilityIdentifier:kSettingsLanguagesCellId];
}

- (TableViewItem*)contentSettingsDetailItem {
  return [self
           detailItemWithType:SettingsItemTypeContentSettings
                         text:l10n_util::GetNSString(
                                  IDS_IOS_CONTENT_SETTINGS_TITLE)
                   detailText:nil
                       symbol:DefaultSettingsRootSymbol(kSettingsFilledSymbol)
        symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
      accessibilityIdentifier:kSettingsContentSettingsCellId];
}

- (TableViewItem*)downloadsSettingsDetailItem {
  return [self detailItemWithType:SettingsItemTypeDownloadsSettings
                             text:l10n_util::GetNSString(
                                      IDS_IOS_SETTINGS_DOWNLOADS_TITLE)
                       detailText:nil
                           symbol:DefaultSettingsRootSymbol(kDownloadSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
          accessibilityIdentifier:kSettingsDownloadsSettingsCellId];
}

- (TableViewItem*)tabsSettingsDetailItem {
  return [self detailItemWithType:SettingsItemTypeTabs
                             text:l10n_util::GetNSString(
                                      IDS_IOS_TABS_MANAGEMENT_SETTINGS)
                       detailText:nil
                           symbol:DefaultSettingsRootSymbol(kTabsSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kOrange500Color]
          accessibilityIdentifier:kSettingsTabsCellId];
}

- (TableViewItem*)bandwidthManagementDetailItem {
  return [self detailItemWithType:SettingsItemTypeBandwidth
                             text:l10n_util::GetNSString(
                                      IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS)
                       detailText:nil
                           symbol:DefaultSettingsRootSymbol(kWifiSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
          accessibilityIdentifier:kSettingsBandwidthCellId];
}

- (TableViewItem*)aboutChromeDetailItem {
  return [self detailItemWithType:SettingsItemTypeAboutChrome
                             text:l10n_util::GetNSString(IDS_IOS_PRODUCT_NAME)
                       detailText:nil
                           symbol:DefaultSettingsRootSymbol(kInfoCircleSymbol)
            symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
          accessibilityIdentifier:kSettingsAboutCellId];
}

- (TableViewSwitchItem*)showMemoryDebugSwitchItem {
  TableViewSwitchItem* showMemoryDebugSwitchItem =
      [self switchItemWithType:SettingsItemTypeMemoryDebugging
                            title:@"Show memory debug tools"
                           symbol:DefaultSettingsRootSymbol(@"memorychip")
            symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
          accessibilityIdentifier:nil];
  showMemoryDebugSwitchItem.on = [_showMemoryDebugToolsEnabled value];

  return showMemoryDebugSwitchItem;
}

- (TableViewItem*)switchProfileItem {
  NSString* detailText = nil;
  std::string profileName = _profile->GetProfileName();
  // TODO(crbug.com/331783685): Remove assumption that "Default" is the
  // personal profile.
  if (profileName == kIOSChromeInitialProfile) {
    detailText = @"Personal";
  } else {
    detailText = base::SysUTF8ToNSString(profileName);
  }
  return [self
           detailItemWithType:SettingsItemTypeSwitchProfile
                         text:l10n_util::GetNSString(
                                  IDS_IOS_SWITCH_PROFILE_MANAGEMENT_SETTINGS)
                   detailText:detailText
                       symbol:DefaultSettingsRootSymbol(kMultiIdentitySymbol)
        symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
      accessibilityIdentifier:nil];
}

- (TableViewItem*)enhancedSafeBrowsingInlinePromoItem {
  EnhancedSafeBrowsingInlinePromoItem* item =
      [[EnhancedSafeBrowsingInlinePromoItem alloc]
          initWithType:SettingsItemTypeESBPromo];
  item.delegate = self;
  return item;
}

#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

- (TableViewSwitchItem*)viewSourceSwitchItem {
  UIImage* image;
  image = DefaultSettingsRootSymbol(@"keyboard.badge.eye");
  TableViewSwitchItem* viewSourceItem =
      [self switchItemWithType:SettingsItemTypeViewSource
                            title:@"View source menu"
                           symbol:image
            symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
          accessibilityIdentifier:nil];

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  viewSourceItem.on = [defaults boolForKey:kDevViewSourceKey];
  return viewSourceItem;
}

- (TableViewDetailIconItem*)tableViewCatalogDetailItem {
  return [self detailItemWithType:SettingsItemTypeTableCellCatalog
                             text:@"TableView Cell Catalog"
                       detailText:nil
                           symbol:DefaultSettingsRootSymbol(@"cart")
            symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
          accessibilityIdentifier:nil];
}
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

#pragma mark Item Constructors

- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                          text:(NSString*)text
                                    detailText:(NSString*)detailText
                                        symbol:(UIImage*)symbol
                         symbolBackgroundColor:(UIColor*)backgroundColor
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.detailText = detailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;
  detailItem.iconImage = symbol;
  if (backgroundColor) {
    detailItem.iconBackgroundColor = backgroundColor;
    detailItem.iconTintColor = UIColor.whiteColor;
  }
  detailItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
  return detailItem;
}

- (TableViewSwitchItem*)switchItemWithType:(NSInteger)type
                                     title:(NSString*)title
                                    symbol:(UIImage*)symbol
                     symbolBackgroundColor:(UIColor*)backgroundColor
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:type];
  switchItem.text = title;
  switchItem.iconImage = symbol;
  switchItem.iconTintColor = UIColor.whiteColor;
  switchItem.iconBackgroundColor = backgroundColor;
  switchItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
  switchItem.accessibilityIdentifier = accessibilityIdentifier;

  return switchItem;
}

- (TableViewInfoButtonItem*)infoButtonWithType:(NSInteger)type
                                          text:(NSString*)text
                                        status:(NSString*)status
                                         image:(UIImage*)image
                               imageBackground:(UIColor*)imageBackground
                             accessibilityHint:(NSString*)accessibilityHint
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  TableViewInfoButtonItem* infoButton =
      [[TableViewInfoButtonItem alloc] initWithType:type];
  infoButton.text = text;
  infoButton.statusText = status;
  if (image) {
    infoButton.iconImage = image;
    DCHECK(imageBackground);
    infoButton.iconBackgroundColor = imageBackground;
    infoButton.iconTintColor = UIColor.whiteColor;
    infoButton.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
  }
  infoButton.accessibilityHint = accessibilityHint;
  infoButton.accessibilityIdentifier = accessibilityIdentifier;
  return infoButton;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if (_settingsAreDismissed)
    return cell;
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case SettingsItemTypeMemoryDebugging: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(memorySwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case SettingsItemTypeArticlesForYou: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(articlesForYouSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case SettingsItemTypeViewSource: {
#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(viewSourceSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
#else
      NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
      break;
    }
    case SettingsItemTypeManagedDefaultSearchEngine: {
      TableViewInfoButtonCell* managedCell =
          base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapManagedUIInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case SettingsItemTypeSigninDisabled: {
      // Adds a trailing button with more information when the sign-in policy
      // has been enabled by the organization.
      TableViewInfoButtonCell* managedCell =
          base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapSigninDisabledInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case SettingsItemTypeManagedArticlesForYou: {
      TableViewInfoButtonCell* managedCell =
          base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapManagedUIInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case SettingsItemTypeGoogleSync: {
      if (![self isSyncDisabledByPolicy])
        break;
      TableViewInfoButtonCell* managedCell =
          base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapSyncDisabledInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_settingsAreDismissed)
    return;

  id object = [self.tableViewModel itemAtIndexPath:indexPath];
  if ([object respondsToSelector:@selector(isEnabled)] &&
      ![object performSelector:@selector(isEnabled)]) {
    // Don't perform any action if the cell isn't enabled.
    return;
  }

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  UIViewController<SettingsRootViewControlling>* controller;

  switch (itemType) {
    case SettingsItemTypeSignInButton:
      signin_metrics::RecordSigninUserActionForAccessPoint(
          signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
      [self showSignIn];
      break;
    case SettingsItemTypeAccount: {
      if ([self shouldReplaceSyncSettingsWithAccountSettings]) {
        // Redirect to Account Settings page if the user is signed-in and
        // not-syncing.
        base::RecordAction(base::UserMetricsAction("Settings.Sync"));
        [self showGoogleSync];
        break;
      }
      base::RecordAction(base::UserMetricsAction("Settings.MyAccount"));

      AccountsCoordinator* accountsCoordinator = [[AccountsCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:_browser
                 closeSettingsOnAddAccount:NO];
      _accountsCoordinator = accountsCoordinator;
      [accountsCoordinator start];
      break;
    }
    case SettingsItemTypeGoogleServices:
      base::RecordAction(base::UserMetricsAction("Settings.GoogleServices"));
      [self showGoogleServices];
      break;
    case SettingsItemTypeGoogleSync: {
      base::RecordAction(base::UserMetricsAction("Settings.Sync"));
      switch (
          GetSyncFeatureState(SyncServiceFactory::GetForProfile(_profile))) {
        case SyncState::kSyncConsentOff: {
          [self showSignIn];
          break;
        }
        case SyncState::kSyncOff: {
          [self showGoogleSync];
          break;
        }
        case SyncState::kSyncEnabled:
        case SyncState::kSyncEnabledWithError:
        case SyncState::kSyncEnabledWithNoSelectedTypes: {
          [self showGoogleSync];
          break;
        }
        case SyncState::kSyncDisabledByAdministrator:
          break;
      }
      break;
    }
    case SettingsItemTypeDefaultBrowser: {
      base::RecordAction(
          base::UserMetricsAction("Settings.ShowDefaultBrowser"));

      if (self.showingDefaultBrowserNotificationDot) {
        feature_engagement::Tracker* tracker =
            feature_engagement::TrackerFactory::GetForProfile(_profile);
        if (tracker) {
          tracker->NotifyEvent(
              feature_engagement::events::kBlueDotPromoSettingsDismissed);
          id<PopupMenuCommands> popupMenuHandler = HandlerForProtocol(
              _browser->GetCommandDispatcher(), PopupMenuCommands);
          [popupMenuHandler updateToolsMenuBlueDotVisibility];
          self.showingDefaultBrowserNotificationDot =
              [popupMenuHandler hasBlueDotForOverflowMenu];
          [self updateDefaultBrowserSettingsBlueDot];
        }
        [self reloadData];
      }

      controller = [[DefaultBrowserSettingsTableViewController alloc] init];
      break;
    }
    case SettingsItemTypeSearchEngine:
      base::RecordAction(base::UserMetricsAction("EditSearchEngines"));
      controller =
          [[SearchEngineTableViewController alloc] initWithProfile:_profile];
      break;
    case SettingsItemTypeAddressBar:
      base::RecordAction(base::UserMetricsAction("Settings.AddressBar.Opened"));
      [self showAddressBarPreferenceSetting];
      // Sets the "new" IPH badge shown count to max so it's not shown again.
      GetApplicationContext()->GetLocalState()->SetInteger(
          prefs::kAddressBarSettingsNewBadgeShownCount, INT_MAX);
      break;
    case SettingsItemTypePasswords:
      base::RecordAction(
          base::UserMetricsAction("Options_ShowPasswordManager"));
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.ManagePasswordsReferrer",
          password_manager::ManagePasswordsReferrer::kChromeSettings);
      [self showPasswords];
      break;
    case SettingsItemTypeAutofillCreditCard:
      base::RecordAction(base::UserMetricsAction("AutofillCreditCardsViewed"));
      controller = [[AutofillCreditCardTableViewController alloc]
          initWithBrowser:_browser];
      break;
    case SettingsItemTypeAutofillProfile:
      base::RecordAction(base::UserMetricsAction("AutofillAddressesViewed"));
      controller =
          [[AutofillProfileTableViewController alloc] initWithBrowser:_browser];
      break;
    case SettingsItemTypeNotifications:
      CHECK([self shouldShowNotificationsSettings]);
      base::RecordAction(base::UserMetricsAction("Settings.Notifications"));
      [self showNotifications];
      break;
    case SettingsItemTypeVoiceSearch:
      base::RecordAction(base::UserMetricsAction("Settings.VoiceSearch"));
      controller = [[VoiceSearchTableViewController alloc]
          initWithPrefs:_profile->GetPrefs()];
      break;
    case SettingsItemTypeSafetyCheck:
      base::RecordAction(base::UserMetricsAction("Settings.SafetyCheck"));
      [self showSafetyCheck];
      break;
    case SettingsItemTypePrivacy:
      base::RecordAction(base::UserMetricsAction("Settings.Privacy"));
      [self showPrivacy];
      break;
    case SettingsItemTypeLanguageSettings: {
      base::RecordAction(base::UserMetricsAction("Settings.Language"));
      language::LanguageModelManager* languageModelManager =
          LanguageModelManagerFactory::GetForProfile(_profile);
      LanguageSettingsMediator* mediator = [[LanguageSettingsMediator alloc]
          initWithLanguageModelManager:languageModelManager
                           prefService:_profile->GetPrefs()];
      LanguageSettingsTableViewController* languageSettingsTableViewController =
          [[LanguageSettingsTableViewController alloc]
              initWithDataSource:mediator
                  commandHandler:mediator];
      mediator.consumer = languageSettingsTableViewController;
      controller = languageSettingsTableViewController;
      break;
    }
    case SettingsItemTypeContentSettings:
      base::RecordAction(base::UserMetricsAction("Settings.ContentSettings"));
      controller =
          [[ContentSettingsTableViewController alloc] initWithBrowser:_browser];
      break;
    case SettingsItemTypeDownloadsSettings:
      base::RecordAction(base::UserMetricsAction("Settings.DownloadsSettings"));
      [self showDownloadsSettings];
      break;
    case SettingsItemTypeTabs:
      base::RecordAction(base::UserMetricsAction("Settings.Tabs"));
      [self showTabsSettings];
      break;
    case SettingsItemTypeBandwidth:
      base::RecordAction(base::UserMetricsAction("Settings.Bandwidth"));
      controller = [[BandwidthManagementTableViewController alloc]
          initWithProfile:_profile];
      break;
    case SettingsItemTypeAboutChrome: {
      base::RecordAction(base::UserMetricsAction("AboutChrome"));
      AboutChromeTableViewController* aboutChromeTableViewController =
          [[AboutChromeTableViewController alloc] init];
      controller = aboutChromeTableViewController;
      break;
    }
    case SettingsItemTypeMemoryDebugging:
    case SettingsItemTypeViewSource:
      // Taps on these don't do anything. They have a switch as accessory view
      // and only the switch is tappable.
      break;
    case SettingsItemTypeTableCellCatalog:
      [self.navigationController
          pushViewController:[[TableCellCatalogViewController alloc] init]
                    animated:YES];
      break;
    case SettingsItemTypePlusAddresses: {
      base::RecordAction(base::UserMetricsAction("Settings.PlusAddresses"));
      OpenNewTabCommand* command = [OpenNewTabCommand
          commandWithURLFromChrome:
              GURL(plus_addresses::features::kPlusAddressManagementUrl.Get())];
      id<ApplicationCommands> handler = HandlerForProtocol(
          _browser->GetCommandDispatcher(), ApplicationCommands);
      [handler closeSettingsUIAndOpenURL:command];
      break;
    }
    case SettingsItemTypeSwitchProfile:
      [self showSwitchProfileSettings];
      break;
    default:
      break;
  }

  if (controller) {
    [self configureHandlersForRootViewController:controller];
    [self.navigationController pushViewController:controller animated:YES];
  }
}

#pragma mark - Actions

// Called when the user taps on the information button of a managed setting's UI
// cell.
- (void)didTapSigninDisabledInfoButton:(UIButton*)buttonView {
  NSString* popoverMessage =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SIGNIN_DISABLED_POPOVER_TEXT);
  EnterpriseInfoPopoverViewController* popover =
      [[EnterpriseInfoPopoverViewController alloc]
          initWithMessage:popoverMessage
           enterpriseName:nil];

  [self showEnterprisePopover:popover forInfoButton:buttonView];
}

// Called when the user taps on the information button of the sync setting
// while sync is disabled by policy.
- (void)didTapSyncDisabledInfoButton:(UIButton*)buttonView {
  NSString* popoverMessage =
      l10n_util::GetNSString(IDS_IOS_SYNC_SETTINGS_DISABLED_POPOVER_TEXT);
  EnterpriseInfoPopoverViewController* popover =
      [[EnterpriseInfoPopoverViewController alloc]
          initWithMessage:popoverMessage
           enterpriseName:nil];

  [self showEnterprisePopover:popover forInfoButton:buttonView];
}

// Called when the user taps on the information button of the sign-in setting
// while sign-in is disabled by policy.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* popover =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];

  [self showEnterprisePopover:popover forInfoButton:buttonView];
}

// Shows a contextual bubble explaining that the tapped setting is managed and
// includes a link to the chrome://management page.
- (void)showEnterprisePopover:(EnterpriseInfoPopoverViewController*)popover
                forInfoButton:(UIButton*)buttonView {
  popover.delegate = self;

  // Disable the button when showing the bubble.
  // The button will be enabled when close the bubble in
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

#pragma mark Switch Actions

- (void)memorySwitchToggled:(UISwitch*)sender {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:SettingsItemTypeMemoryDebugging
                              sectionIdentifier:SettingsSectionIdentifierDebug];

  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [_showMemoryDebugToolsEnabled setValue:newSwitchValue];
}

- (void)articlesForYouSwitchToggled:(UISwitch*)sender {
  base::RecordAction(base::UserMetricsAction("Settings.ArticlesForYouToggled"));

  NSIndexPath* switchPath = [self.tableViewModel
      indexPathForItemType:SettingsItemTypeArticlesForYou
         sectionIdentifier:SettingsSectionIdentifierAdvanced];

  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [_articlesEnabled setValue:newSwitchValue];
}

#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
- (void)viewSourceSwitchToggled:(UISwitch*)sender {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:SettingsItemTypeViewSource
                              sectionIdentifier:SettingsSectionIdentifierDebug];

  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [self setBooleanNSUserDefaultsValue:newSwitchValue forKey:kDevViewSourceKey];
}
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

#pragma mark - Private methods

// Returns true if sync is disabled by policy.
- (bool)isSyncDisabledByPolicy {
  return SyncServiceFactory::GetForProfile(_profile)->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

- (void)showGoogleServices {
  if (_googleServicesSettingsCoordinator &&
      self.navigationController.topViewController != self) {
    base::debug::DumpWithoutCrashing();
  }

  _googleServicesSettingsCoordinator =
      [[GoogleServicesSettingsCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:_browser];
  _googleServicesSettingsCoordinator.delegate = self;
  [_googleServicesSettingsCoordinator start];
}

- (void)showTabsSettings {
  if (_tabsCoordinator && self.navigationController.topViewController != self) {
    base::debug::DumpWithoutCrashing();
  }

  _tabsCoordinator = [[TabsSettingsCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  [_tabsCoordinator start];
}

- (void)showSwitchProfileSettings {
  if (_switchProfileCoordinator &&
      self.navigationController.topViewController != self) {
    base::debug::DumpWithoutCrashing();
  }

  _switchProfileCoordinator = [[SwitchProfileSettingsCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  [_switchProfileCoordinator start];
}

- (void)showAddressBarPreferenceSetting {
  if (_addressBarPreferenceCoordinator &&
      self.navigationController.topViewController != self) {
    base::debug::DumpWithoutCrashing();
  }

  _addressBarPreferenceCoordinator = [[AddressBarPreferenceCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  [_addressBarPreferenceCoordinator start];
}

- (BOOL)shouldReplaceSyncSettingsWithAccountSettings {
  // TODO(crbug.com/40066949): Remove usage of HasSyncConsent() after kSync
  // users migrated to kSignin in phase 3. See ConsentLevel::kSync
  // documentation for details.
  return !SyncServiceFactory::GetForProfile(_profile)->HasSyncConsent();
}

- (void)showGoogleSync {
  if (_manageSyncSettingsCoordinator &&
      self.navigationController.topViewController != self) {
    base::debug::DumpWithoutCrashing();
  }

  SyncSettingsAccountState accountState =
      [self shouldReplaceSyncSettingsWithAccountSettings]
          ? SyncSettingsAccountState::kSignedIn
          : SyncSettingsAccountState::kSyncing;
  _manageSyncSettingsCoordinator = [[ManageSyncSettingsCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser
                          accountState:accountState];
  _manageSyncSettingsCoordinator.delegate = self;
  [_manageSyncSettingsCoordinator start];
}

- (void)showPasswords {
  if (_passwordsCoordinator &&
      self.navigationController.topViewController != self) {
    base::debug::DumpWithoutCrashing();
  }

  _passwordsCoordinator = [[PasswordsCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  _passwordsCoordinator.delegate = self;
  [_passwordsCoordinator start];
}

// Shows the Safety Check screen.
- (void)showSafetyCheck {
  if (_safetyCheckCoordinator &&
      self.navigationController.topViewController != self) {
    base::debug::DumpWithoutCrashing();
  }

  _safetyCheckCoordinator = [[SafetyCheckCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser
                              referrer:password_manager::PasswordCheckReferrer::
                                           kSafetyCheck];
  _safetyCheckCoordinator.delegate = self;
  [_safetyCheckCoordinator start];
}

// Checks if there are any remaining password issues that are not muted from the
// last time password check was run.
- (BOOL)hasPasswordIssuesRemaining {
  if (!_passwordCheckManager) {
    return NO;
  }
  return !_passwordCheckManager->GetInsecureCredentials().empty();
}

// Displays a warning icon in the `_safetyCheckItem` if there is a reamining
// issue for any of the safety checks.
- (void)updateSafetyCheckItemTrailingIcon {
  if (!_safetyCheckItem || !_passwordCheckManager) {
    return;
  }

  if (!PreviousSafetyCheckIssueFound()) {
    _safetyCheckItem.trailingImage = nil;
    _safetyCheckItem.trailingImageTintColor = nil;
    return;
  }

  if (!IsAppUpToDate()) {
    _safetyCheckItem.warningState = WarningState::kSevereWarning;
  } else if ([self hasPasswordIssuesRemaining]) {
    password_manager::WarningType warningType = GetWarningOfHighestPriority(
        _passwordCheckManager->GetInsecureCredentials());
    if (warningType ==
        password_manager::WarningType::kCompromisedPasswordsWarning) {
      _safetyCheckItem.warningState = WarningState::kSevereWarning;
    } else {
      // Getting here means that there are reused, weak and/or muted passwords.
      // In Safety Check, an icon is shown for passwords only when all passwords
      // are safe or when there are unmuted compromised passwords. When there
      // are reused, weak and/or muted passwords, no icon is shown.
      _safetyCheckItem.trailingImage = nil;
    }
  }
  [self reconfigureCellsForItems:@[ _safetyCheckItem ]];
}

// Shows Notifications screen.
- (void)showNotifications {
  if (_notificationsCoordinator &&
      self.navigationController.topViewController != self) {
    base::debug::DumpWithoutCrashing();
  }

  _notificationsCoordinator = [[NotificationsCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  _notificationsCoordinator.delegate = self;
  [_notificationsCoordinator start];
}

// Shows Privacy screen.
- (void)showPrivacy {
  if (_privacyCoordinator &&
      self.navigationController.topViewController != self) {
    base::debug::DumpWithoutCrashing();
  }

  _privacyCoordinator = [[PrivacyCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  _privacyCoordinator.delegate = self;
  [_privacyCoordinator start];
}

// Sets the NSUserDefaults BOOL `value` for `key`.
- (void)setBooleanNSUserDefaultsValue:(BOOL)value forKey:(NSString*)key {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:value forKey:key];
  [defaults synchronize];
}

// Returns YES if a "Debug" section should be shown. This is always true for
// Chromium builds, but for official builds it is gated by an experimental flag
// because the "Debug" section should never be showing in stable channel.
- (BOOL)hasDebugSection {
#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
  return YES;
#else
  if (experimental_flags::IsMemoryDebuggingEnabled()) {
    return YES;
  }
  return NO;
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
}

// Updates the identity cell.
- (void)updateIdentityAccountItem:(TableViewAccountItem*)identityAccountItem {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(_profile);
  _identity = authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!_identity) {
    // This could occur during the sign out process. Just ignore as the account
    // cell will be replaced by the "Sign in" button.
    return;
  }
  identityAccountItem.image =
      self.accountManagerService->GetIdentityAvatarWithIdentity(
          _identity, IdentityAvatarSize::TableViewIcon);
  identityAccountItem.text = _identity.userFullName;
  identityAccountItem.detailText = _identity.userEmail;

  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(_profile);
  DCHECK(syncService);
  identityAccountItem.shouldDisplayError =
      GetAccountErrorUIInfo(syncService) != nil;
}

- (void)reloadAccountCell {
  if (![self.tableViewModel
          hasItemForItemType:SettingsItemTypeAccount
           sectionIdentifier:SettingsSectionIdentifierAccount]) {
    return;
  }
  NSIndexPath* accountCellIndexPath = [self.tableViewModel
      indexPathForItemType:SettingsItemTypeAccount
         sectionIdentifier:SettingsSectionIdentifierAccount];
  TableViewAccountItem* identityAccountItem =
      base::apple::ObjCCast<TableViewAccountItem>(
          [self.tableViewModel itemAtIndexPath:accountCellIndexPath]);
  if (identityAccountItem) {
    [self updateIdentityAccountItem:identityAccountItem];
    [self reconfigureCellsForItems:@[ identityAccountItem ]];
  }
}

- (void)maybeShowSigninIPH {
  if (_settingsAreDismissed) {
    return;
  }
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(_browser->GetProfile());
  BOOL shouldShowSigninIPH =
      authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin) &&
      [self shouldReplaceSyncSettingsWithAccountSettings];
  if (!shouldShowSigninIPH) {
    return;
  }

  UITableViewCell* accountCell = nil;
  for (UITableViewCell* cell in [self.tableView visibleCells]) {
    if ([cell isKindOfClass:[TableViewAccountCell class]]) {
      accountCell = cell;
      break;
    }
  }
  if (!accountCell) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType dismissReason,
        feature_engagement::Tracker::SnoozeAction snoozeAction) {
        [weakSelf signinIPHDismissed];
      };
  _bubblePresenter = [[BubbleViewControllerPresenter alloc]
           initWithText:l10n_util::GetNSString(IDS_IOS_SETTING_IPH_SIGNIN)
                  title:nil
                  image:nil
         arrowDirection:BubbleArrowDirectionUp
              alignment:BubbleAlignmentCenter
             bubbleType:BubbleViewTypeDefault
      dismissalCallback:dismissalCallback];
  CGPoint anchorPointInCell = CGPointMake(CGRectGetMidX(accountCell.bounds),
                                          CGRectGetMaxY(accountCell.bounds));
  CGPoint anchorPointInWindow = [self.view.window convertPoint:anchorPointInCell
                                                      fromView:accountCell];

  // The IPH must be presented if
  // `_featureEngagementTracker->ShouldTriggerHelpUI()` returns true.
  BOOL canShowSigninIPH =
      [_bubblePresenter canPresentInView:self.view
                             anchorPoint:anchorPointInWindow] &&
      _featureEngagementTracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHiOSReplaceSyncPromosWithSignInPromos);
  if (canShowSigninIPH) {
    [_bubblePresenter presentInViewController:self
                                  anchorPoint:anchorPointInWindow];
  } else {
    _bubblePresenter = nil;
  }
}

- (void)signinIPHDismissed {
  _featureEngagementTracker->Dismissed(
      feature_engagement::kIPHiOSReplaceSyncPromosWithSignInPromos);
  _bubblePresenter = nil;
}

// Updates the Sync item to display the right icon and status message in the
// cell.
- (void)updateSyncItem:(TableViewDetailIconItem*)googleSyncItem {
  switch (GetSyncFeatureState(SyncServiceFactory::GetForProfile(_profile))) {
    case SyncState::kSyncConsentOff: {
      googleSyncItem.detailText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
      googleSyncItem.iconImage = CustomSettingsRootSymbol(kSyncDisabledSymbol);
      googleSyncItem.iconBackgroundColor = [UIColor colorNamed:kGrey400Color];
      googleSyncItem.iconTintColor = UIColor.whiteColor;
      googleSyncItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
      break;
    }
    case SyncState::kSyncOff:
    case SyncState::kSyncEnabledWithNoSelectedTypes: {
      googleSyncItem.detailText = nil;
      googleSyncItem.iconImage = CustomSettingsRootSymbol(kSyncDisabledSymbol);
      googleSyncItem.iconBackgroundColor = [UIColor colorNamed:kGrey400Color];
      googleSyncItem.iconTintColor = UIColor.whiteColor;
      googleSyncItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
      break;
    }
    case SyncState::kSyncEnabledWithError: {
      syncer::SyncService* syncService =
          SyncServiceFactory::GetForProfile(_profile);
      googleSyncItem.detailText =
          GetSyncErrorDescriptionForSyncService(syncService);
      googleSyncItem.iconImage = DefaultSettingsRootSymbol(kSyncErrorSymbol);
      googleSyncItem.iconBackgroundColor = [UIColor colorNamed:kRed500Color];
      googleSyncItem.iconTintColor = UIColor.whiteColor;
      googleSyncItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
      // Return a vertical layout of title / subtitle in the case of a sync
      // error.
      googleSyncItem.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
      return;
    }
    case SyncState::kSyncEnabled: {
      googleSyncItem.detailText = l10n_util::GetNSString(IDS_IOS_SETTING_ON);

      googleSyncItem.iconImage = DefaultSettingsRootSymbol(kSyncEnabledSymbol);
      googleSyncItem.iconBackgroundColor = [UIColor colorNamed:kGreen500Color];
      googleSyncItem.iconTintColor = UIColor.whiteColor;
      googleSyncItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
      break;
    }
    case SyncState::kSyncDisabledByAdministrator:
      // Nothing to update.
      break;
  }
  // Needed to update the item text layout in the case that it was previously
  // set to UILayoutConstraintAxisVertical due to a sync error.
  googleSyncItem.textLayoutConstraintAxis = UILayoutConstraintAxisHorizontal;
}

// Check if the default search engine is managed by policy.
- (BOOL)isDefaultSearchEngineManagedByPolicy {
  const base::Value::Dict& dict = _profile->GetPrefs()->GetDict(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);

  if (dict.FindBoolByDottedPath(DefaultSearchManager::kDisabledByPolicy) ||
      dict.FindBoolByDottedPath(prefs::kDefaultSearchProviderEnabled))
    return YES;
  return NO;
}

// Returns the text to be displayed by the managed Search Engine item.
- (NSString*)managedSearchEngineDetailText {
  const base::Value::Dict& dict = _profile->GetPrefs()->GetDict(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  if (dict.FindBoolByDottedPath(DefaultSearchManager::kDisabledByPolicy)) {
    // Default search engine is disabled by policy.
    return l10n_util::GetNSString(
        IDS_IOS_SEARCH_ENGINE_SETTING_DISABLED_STATUS);
  }
  // Default search engine is enabled and set by policy.
  const std::string* status =
      dict.FindStringByDottedPath(DefaultSearchManager::kShortName);
  return base::SysUTF8ToNSString(*status);
}

// Returns the appropriate text to update the title for the feed item.
- (NSString*)feedItemTitle {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(_browser->GetProfile());
  BOOL isSignedIn =
      authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
  return (isSignedIn && IsWebChannelsEnabled())
             ? l10n_util::GetNSString(IDS_IOS_DISCOVER_AND_FOLLOWING_FEED_TITLE)
             : l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE);
}

// Decides whether the default browser blue dot promo should be active, and adds
// the blue dot badge to the right settings row if it is.
- (void)updateDefaultBrowserSettingsBlueDot {
  // Add or remove the blue dot promo badge for the default browser row.
  if (self.showingDefaultBrowserNotificationDot) {
    _defaultBrowserCellItem.badgeType = BadgeType::kNotificationDot;
  } else {
    _defaultBrowserCellItem.badgeType = BadgeType::kNone;
  }
}

// Add or remove the "new" IPH badge from the address bar settings row. The
// badge is shown a maximum of `kMaxShowCountNewIPHBadge` times.
- (void)updateAddressBarNewIPHBadge {
  CHECK(_addressBarPreferenceItem);

  PrefService* prefService = GetApplicationContext()->GetLocalState();
  NSInteger showCount =
      prefService->GetInteger(prefs::kAddressBarSettingsNewBadgeShownCount);

  BadgeType badgeType = BadgeType::kNone;

  const BOOL isFreshInstall = IsFirstRunRecent(kFreshInstallTimeDelta);

  if (!isFreshInstall && showCount < kMaxShowCountNewIPHBadge) {
    badgeType = BadgeType::kNew;
    prefService->SetInteger(prefs::kAddressBarSettingsNewBadgeShownCount,
                            showCount + 1);
  }

  if (badgeType != _addressBarPreferenceItem.badgeType) {
    _addressBarPreferenceItem.badgeType = badgeType;
    [self reconfigureCellsForItems:@[ _addressBarPreferenceItem ]];
  }
}

// Updates the state of the Safety Check notifications button based on whether
// the user has Safety Check notifications enabled.
- (void)updateSafetyCheckNotificationsButtonState {
  CHECK(IsSafetyCheckNotificationsEnabled());

  // Safety Check notifications are controlled by app-wide notification
  // settings, not profile-specific ones. No Gaia ID is required below in
  // `GetMobileNotificationPermissionStatusForClient()`.
  BOOL enabled = push_notification_settings::
      GetMobileNotificationPermissionStatusForClient(
          PushNotificationClientId::kSafetyCheck, "");

  [_safetyCheckCoordinator updateNotificationsButton:enabled];
}

// Updates the string indicating the push notification state.
- (void)updateNotificationsDetailText {
  if (!_notificationsItem) {
    return;
  }

  NSString* detailText = nil;
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(_profile);
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  PrefService* prefService = _profile->GetPrefs();
  const std::string& gaiaID = base::SysNSStringToUTF8(identity.gaiaID);
  push_notification_settings::ClientPermissionState permission_state =
      push_notification_settings::GetNotificationPermissionState(gaiaID,
                                                                 prefService);
  if (permission_state ==
      push_notification_settings::ClientPermissionState::ENABLED) {
    detailText = l10n_util::GetNSString(IDS_IOS_SETTING_ON);
  } else if (permission_state ==
             push_notification_settings::ClientPermissionState::DISABLED) {
    detailText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  }

  _notificationsItem.detailText = detailText;
  [self reconfigureCellsForItems:@[ _notificationsItem ]];
}

- (void)showDownloadsSettings {
  if (_downloadsSettingsCoordinator &&
      self.navigationController.topViewController != self) {
    base::debug::DumpWithoutCrashing();
  }

  _downloadsSettingsCoordinator = [[DownloadsSettingsCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  _downloadsSettingsCoordinator.delegate = self;
  [_downloadsSettingsCoordinator start];
}

// Returns YES if the Notifications settings should show.
- (BOOL)shouldShowNotificationsSettings {
  return base::FeatureList::IsEnabled(kNotificationSettingsMenuItem) &&
         (IsPriceNotificationsEnabled() ||
          IsContentNotificationEnabled(_profile) ||
          IsIOSTipsNotificationsEnabled() ||
          base::FeatureList::IsEnabled(
              send_tab_to_self::kSendTabToSelfIOSPushNotifications));
}

// Records that the user has reached the impression limit for the enhanced safe
// browsing inline promo.
- (void)maybeRecordEnhancedSafeBrowsingImpressionLimitReached {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(_profile);
  std::vector<std::pair<feature_engagement::EventConfig, int>> events =
      tracker->ListEvents(
          feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature);
  for (const auto& event : events) {
    if (event.first.name == "inline_enhanced_safe_browsing_promo_trigger") {
      unsigned int impressionLimit = event.first.comparator.value;
      unsigned int numberOfImpressions = event.second;
      if (impressionLimit == numberOfImpressions) {
        base::RecordAction(base::UserMetricsAction(
            "MobileSettingsEnhancedSafeBrowsingInlineProm"
            "oImpressionLimitReached"));
      }
    }
  }
}

// Returns YES if the Enhanced Safe Browsing inline promo should show.
- (BOOL)shouldShowEnhancedSafeBrowsingPromo {
  // First check if another active settings page (e.g. in another
  // window) has an active promo. If so, just return that the promo should be
  // shown here without querying the FET. Only query the FET if there is no
  // currently active promo.
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(_profile);
  EnhancedSafeBrowsingActivePromoData* data =
      static_cast<EnhancedSafeBrowsingActivePromoData*>(
          tracker->GetUserData(EnhancedSafeBrowsingActivePromoData::key));
  if (data) {
    data->active_promos++;
    return YES;
  }

  // The user must be:
  //   1.) Signed-in
  //   2.) Have Chrome set to default browser.
  //   3.) Have Safe Browsing standard protection enabled.
  //   4.) One of the trigerring criteria has been met.
  //   5.) Not have their Safe Browsing preferences enterprise-managed.
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(_profile);
  bool isSignedInAndSynced =
      authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
  bool isDefaultBrowser = IsChromeLikelyDefaultBrowser();
  bool isStandardProtectionEnabled =
      safe_browsing::GetSafeBrowsingState(*_profile->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION;
  bool triggerCriteriaMet = tracker->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature);
  bool isEnterpriseManaged =
      safe_browsing::IsSafeBrowsingPolicyManaged(*_profile->GetPrefs());

  if (!isSignedInAndSynced || !isDefaultBrowser ||
      !isStandardProtectionEnabled || !triggerCriteriaMet ||
      isEnterpriseManaged) {
    return NO;
  }

  bool promoIsTriggered = tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature);
  CHECK(promoIsTriggered, base::NotFatalUntil::M131);

  std::unique_ptr<EnhancedSafeBrowsingActivePromoData> new_data =
      std::make_unique<EnhancedSafeBrowsingActivePromoData>();
  new_data->active_promos++;
  tracker->SetUserData(EnhancedSafeBrowsingActivePromoData::key,
                       std::move(new_data));

  return YES;
}

// Check if this is the last active Enhanced Safe Browsing promo shown and
// dismisses the FET if so.
- (void)removeEnhancedSafeBrowsingPromoFETDataIfNeeded {
  CHECK(_profile, base::NotFatalUntil::M131);
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(_profile);
  EnhancedSafeBrowsingActivePromoData* data =
      static_cast<EnhancedSafeBrowsingActivePromoData*>(
          tracker->GetUserData(EnhancedSafeBrowsingActivePromoData::key));
  if (!data) {
    return;
  }

  data->active_promos--;
  if (data->active_promos <= 0) {
    tracker->RemoveUserData(EnhancedSafeBrowsingActivePromoData::key);
    tracker->Dismissed(
        feature_engagement::kIPHiOSInlineEnhancedSafeBrowsingPromoFeature);
  }
}

#pragma mark - Sign in

- (void)showSignIn {
  if (self.isSigninInProgress) {
    // According to crbug.com/1498153, it is possible for the user to tap twice
    // on the sign-in cell from the settings to open the sign-in dialog.
    // If this happens, the second tap should ignored.
    return;
  }
  self.isSigninInProgress = YES;
  __weak __typeof(self) weakSelf = self;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kSheetSigninAndHistorySync
               identity:nil
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(SigninCoordinatorResult result,
                          SigninCompletionInfo* completionInfo) {
                 [weakSelf didFinishSignin];
               }];
  [self.applicationHandler showSignin:command baseViewController:self];
}

- (void)didFinishSignin {
  if (_settingsAreDismissed) {
    return;
  }

  // The sign-in is done. The sign-in promo cell or account cell can be
  // reloaded.
  DCHECK(self.isSigninInProgress);
  self.isSigninInProgress = NO;
  [self reloadData];

  // Post the task to show signin IPH so that the UI has had time to refresh
  // after `reloadData`.
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf maybeShowSigninIPH];
      }));
}

#pragma mark SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileSettingsClose"));
}

- (void)reportBackUserAction {
  // Not called for root settings controller.
  NOTREACHED_IN_MIGRATION();
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // Remove Enhanced Safe Browsing Promo.
  [self removeEnhancedSafeBrowsingPromoFETDataIfNeeded];

  // Stop children coordinators.
  [_googleServicesSettingsCoordinator stop];
  _googleServicesSettingsCoordinator.delegate = nil;
  _googleServicesSettingsCoordinator = nil;

  [_safetyCheckCoordinator stop];
  _safetyCheckCoordinator = nil;

  [_passwordsCoordinator stop];
  _passwordsCoordinator.delegate = nil;
  _passwordsCoordinator = nil;

  [_accountsCoordinator stop];
  _accountsCoordinator = nil;

  [_notificationsCoordinator stop];
  _notificationsCoordinator = nil;

  [_privacyCoordinator stop];
  _privacyCoordinator = nil;

  [_manageSyncSettingsCoordinator stop];
  _manageSyncSettingsCoordinator = nil;

  [_tabsCoordinator stop];
  _tabsCoordinator = nil;

  [_switchProfileCoordinator stop];
  _switchProfileCoordinator = nil;

  [_addressBarPreferenceCoordinator stop];
  _addressBarPreferenceCoordinator = nil;

  [_downloadsSettingsCoordinator stop];
  _downloadsSettingsCoordinator = nil;

  // Stop observable prefs.
  [_showMemoryDebugToolsEnabled stop];
  [_showMemoryDebugToolsEnabled setObserver:nil];
  _showMemoryDebugToolsEnabled = nil;

  [_articlesEnabled stop];
  [_articlesEnabled setObserver:nil];
  _articlesEnabled = nil;

  [_allowChromeSigninPreference stop];
  [_allowChromeSigninPreference setObserver:nil];
  _allowChromeSigninPreference = nil;

  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;

  [_contentSuggestionPolicyEnabled stop];
  [_contentSuggestionPolicyEnabled setObserver:nil];
  _contentSuggestionPolicyEnabled = nil;

  [_contentSuggestionForSupervisedUsersEnabled stop];
  [_contentSuggestionForSupervisedUsersEnabled setObserver:nil];
  _contentSuggestionForSupervisedUsersEnabled = nil;

  // Remove pref changes registrations.
  _prefChangeRegistrar.RemoveAll();

  // Remove observer bridges.
  _prefObserverBridge.reset();
  _passwordCheckObserver.reset();
  _searchEngineObserverBridge.reset();
  _syncObserverBridge.reset();
  _identityObserverBridge.reset();
  _accountManagerServiceObserver.reset();

  // Remove PrefObserverDelegates.
  _notificationsObserver.delegate = nil;
  [_notificationsObserver disconnect];
  _notificationsObserver = nil;

  // Clear C++ ivars.
  _voiceLocaleCode.Destroy();
  _passwordCheckManager.reset();
  _browser = nullptr;
  _profile = nullptr;

  _settingsAreDismissed = YES;
}

#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
  // Feed settings are subject to sign-in status and account type, ensure
  // that these sections are updated as necessary.
  [self booleanDidChange:_contentSuggestionPolicyEnabled];

  [self updateSigninSection];
  // The Identity section may be added or removed depending on sign-in is
  // allowed. Reload all sections in the model to account for the change.
  [self.tableView reloadData];
}

#pragma mark - SearchEngineObserverBridge

- (void)searchEngineChanged {
  if (_managedSearchEngineItem) {
    _managedSearchEngineItem.statusText = [self managedSearchEngineDetailText];
    [self reconfigureCellsForItems:@[ _managedSearchEngineItem ]];
  } else {
    // The two items are mutually exclusive.
    _defaultSearchEngineItem.detailText =
        base::SysUTF16ToNSString(GetDefaultSearchEngineName(
            ios::TemplateURLServiceFactory::GetForProfile(_profile)));
    [self reconfigureCellsForItems:@[ _defaultSearchEngineItem ]];
  }
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if ([_identity isEqual:identity]) {
    [self reloadAccountCell];
  }
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40926211): settingsWillBeDismissed must be called before the
  // AccountManagerService is destroyed. Switch to DCHECK if the number of
  // reports is low.
  DUMP_WILL_BE_CHECK(!_accountManagerServiceObserver.get());
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _showMemoryDebugToolsEnabled) {
    // Update the Item.
    _showMemoryDebugToolsItem.on = [_showMemoryDebugToolsEnabled value];
    // Update the Cell.
    [self reconfigureCellsForItems:@[ _showMemoryDebugToolsItem ]];
  } else if (observableBoolean == _allowChromeSigninPreference) {
    [self updateSigninSection];
    // The Identity section may be added or removed depending on sign-in is
    // allowed. Reload all sections in the model to account for the change.
    [self.tableView reloadData];
  } else if (observableBoolean == _articlesEnabled) {
    self.feedSettingsItem.on = [_articlesEnabled value];
    [self reconfigureCellsForItems:@[ self.feedSettingsItem ]];
  } else if (observableBoolean == _contentSuggestionPolicyEnabled) {
    NSIndexPath* itemIndexPath;
    NSInteger itemTypeToRemove;
    TableViewItem* itemToAdd;
    if ([_contentSuggestionPolicyEnabled value]) {
      if (![self.tableViewModel hasItem:self.managedFeedSettingsItem]) {
        return;
      }
      itemIndexPath =
          [self.tableViewModel indexPathForItem:self.managedFeedSettingsItem];
      itemTypeToRemove = SettingsItemTypeManagedArticlesForYou;
      itemToAdd = self.feedSettingsItem;
    } else {
      if (![self.tableViewModel hasItem:self.feedSettingsItem]) {
        return;
      }
      itemIndexPath =
          [self.tableViewModel indexPathForItem:self.feedSettingsItem];
      itemTypeToRemove = SettingsItemTypeArticlesForYou;
      itemToAdd = self.managedFeedSettingsItem;
    }
    [self.tableViewModel removeItemWithType:itemTypeToRemove
                  fromSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
    [self.tableViewModel insertItem:itemToAdd
            inSectionWithIdentifier:SettingsSectionIdentifierAdvanced
                            atIndex:itemIndexPath.row];
    [self.tableView reloadRowsAtIndexPaths:@[ itemIndexPath ]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
  } else if (observableBoolean == _contentSuggestionForSupervisedUsersEnabled) {
    if ([_contentSuggestionForSupervisedUsersEnabled value]) {
      // Reset Feed settings back on the content suggestion policy.
      [self booleanDidChange:_contentSuggestionPolicyEnabled];
      return;
    }
    NSInteger itemTypeToRemove;
    if ([self.tableViewModel hasItem:self.feedSettingsItem]) {
      itemTypeToRemove = SettingsItemTypeArticlesForYou;
    } else if ([self.tableViewModel hasItem:self.managedFeedSettingsItem]) {
      itemTypeToRemove = SettingsItemTypeManagedArticlesForYou;
    } else {
      return;
    }
    [self.tableViewModel removeItemWithType:itemTypeToRemove
                  fromSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
    NSUInteger index = [self.tableViewModel
        sectionForSectionIdentifier:SettingsSectionIdentifierAdvanced];
    [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
  } else if (observableBoolean == _bottomOmniboxEnabled) {
    _addressBarPreferenceItem.detailText =
        [_bottomOmniboxEnabled value]
            ? l10n_util::GetNSString(IDS_IOS_BOTTOM_ADDRESS_BAR_OPTION)
            : l10n_util::GetNSString(IDS_IOS_TOP_ADDRESS_BAR_OPTION);
    [self reconfigureCellsForItems:@[ _addressBarPreferenceItem ]];
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  [self updateSafetyCheckItemTrailingIcon];
}

- (void)insecureCredentialsDidChange {
  [self updateSafetyCheckItemTrailingIcon];
}

- (void)passwordCheckManagerWillShutdown {
  _passwordCheckObserver.reset();
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kVoiceSearchLocale) {
    voice::SpeechInputLocaleConfig* localeConfig =
        voice::SpeechInputLocaleConfig::GetInstance();
    voice::SpeechInputLocale locale =
        _voiceLocaleCode.GetValue().length()
            ? localeConfig->GetLocaleForCode(_voiceLocaleCode.GetValue())
            : localeConfig->GetDefaultLocale();
    NSString* languageName = base::SysUTF16ToNSString(locale.display_name);
    _voiceSearchDetailItem.detailText = languageName;
    [self reconfigureCellsForItems:@[ _voiceSearchDetailItem ]];
  }

  if (preferenceName == password_manager::prefs::kCredentialsEnableService) {
    BOOL passwordsEnabled = _profile->GetPrefs()->GetBoolean(preferenceName);
    NSString* passwordsDetail =
        passwordsEnabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                         : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _passwordsDetailItem.detailText = passwordsDetail;
    [self reconfigureCellsForItems:@[ _passwordsDetailItem ]];
  }

  if (preferenceName == autofill::prefs::kAutofillProfileEnabled) {
    BOOL autofillProfileEnabled =
        autofill::prefs::IsAutofillProfileEnabled(_profile->GetPrefs());
    NSString* detailText = autofillProfileEnabled
                               ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                               : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _autoFillProfileDetailItem.detailText = detailText;
    [self reconfigureCellsForItems:@[ _autoFillProfileDetailItem ]];
  }

  if (preferenceName == autofill::prefs::kAutofillCreditCardEnabled) {
    BOOL autofillCreditCardEnabled =
        autofill::prefs::IsAutofillPaymentMethodsEnabled(_profile->GetPrefs());
    NSString* detailText = autofillCreditCardEnabled
                               ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                               : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _autoFillCreditCardDetailItem.detailText = detailText;
    [self reconfigureCellsForItems:@[ _autoFillCreditCardDetailItem ]];
  }

  if (preferenceName ==
      DefaultSearchManager::kDefaultSearchProviderDataPrefName) {
    // Reloading the data is needed because the item type and its class are
    // changing.
    [self reloadData];
  }
}

#pragma mark - GoogleServicesSettingsCoordinatorDelegate

- (void)googleServicesSettingsCoordinatorDidRemove:
    (GoogleServicesSettingsCoordinator*)coordinator {
  DCHECK_EQ(_googleServicesSettingsCoordinator, coordinator);
  [_googleServicesSettingsCoordinator stop];
  _googleServicesSettingsCoordinator.delegate = nil;
  _googleServicesSettingsCoordinator = nil;
}

#pragma mark - SafetyCheckCoordinatorDelegate

- (void)safetyCheckCoordinatorDidRemove:(SafetyCheckCoordinator*)coordinator {
  DCHECK_EQ(_safetyCheckCoordinator, coordinator);
  [_safetyCheckCoordinator stop];
  _safetyCheckCoordinator.delegate = nil;
  _safetyCheckCoordinator = nil;
}

#pragma mark - PasswordsCoordinatorDelegate

- (void)passwordsCoordinatorDidRemove:(PasswordsCoordinator*)coordinator {
  DCHECK_EQ(_passwordsCoordinator, coordinator);
  [_passwordsCoordinator stop];
  _passwordsCoordinator.delegate = nil;
  _passwordsCoordinator = nil;
}

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  // Pop everything up to the Settings page.
  // When there is content presented, don't animate the dismissal of the view
  // controllers in the navigation controller to prevent revealing passwords
  // when the presented content is the one covered by the reauthentication UI.

  UINavigationController* navigationController = self.navigationController;
  UIViewController* topViewController = navigationController.topViewController;
  UIViewController* presentedViewController =
      topViewController.presentedViewController;

  [navigationController popToViewController:self
                                   animated:presentedViewController == nil];

  [presentedViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

#pragma mark - NotificationsCoordinatorDelegate

- (void)notificationsCoordinatorDidRemove:
    (NotificationsCoordinator*)coordinator {
  DCHECK_EQ(_notificationsCoordinator, coordinator);
  [_notificationsCoordinator stop];
  _notificationsCoordinator = nil;
}

#pragma mark - PrivacyCoordinatorDelegate

- (void)privacyCoordinatorViewControllerWasRemoved:
    (PrivacyCoordinator*)coordinator {
  DCHECK_EQ(_privacyCoordinator, coordinator);
  [_privacyCoordinator stop];
  _privacyCoordinator = nil;
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Notifies this controller that the sign in state has changed.
- (void)signinStateDidChange {
  // While the sign-in view is in progress, the TableView should not be
  // updated. Otherwise, it would lead to an UI glitch either while the sign
  // in UI is appearing or disappearing. The TableView will be reloaded once
  // the animation is finished.
  // See: -[SettingsTableViewController didFinishSignin].
  if (self.isSigninInProgress)
    return;
  // Sign in state changes are rare. Just reload the entire table when
  // this happens.
  [self reloadData];
}

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  [self signinStateDidChange];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(base::UserMetricsAction("IOSSettingsCloseWithSwipe"));
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

#pragma mark - ManageSyncSettingsCoordinatorDelegate

- (void)manageSyncSettingsCoordinatorWasRemoved:
    (ManageSyncSettingsCoordinator*)coordinator {
  DCHECK_EQ(_manageSyncSettingsCoordinator, coordinator);
  [_manageSyncSettingsCoordinator stop];
  _manageSyncSettingsCoordinator = nil;
}

- (NSString*)manageSyncSettingsCoordinatorTitle {
  return l10n_util::GetNSString(IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE);
}

#pragma mark - NotificationsSettingsObserverDelegate

- (void)notificationsSettingsDidChangeForClient:
    (PushNotificationClientId)clientID {
  [self updateNotificationsDetailText];

  if (IsSafetyCheckNotificationsEnabled() &&
      clientID == PushNotificationClientId::kSafetyCheck) {
    [self updateSafetyCheckNotificationsButtonState];
  }
}

#pragma mark - DownloadsSettingsCoordinatorDelegate

- (void)downloadsSettingsCoordinatorWasRemoved:
    (DownloadsSettingsCoordinator*)coordinator {
  [_downloadsSettingsCoordinator stop];
  _downloadsSettingsCoordinator = nil;
}

#pragma mark - EnhancedSafeBrowsingInlinePromoDelegate

- (void)dismissEnhancedSafeBrowsingInlinePromo {
  SettingsSectionIdentifier sectionID = SettingsSectionIdentifierESBPromo;
  if (![self.tableViewModel hasSectionForSectionIdentifier:sectionID]) {
    return;
  }

  NSUInteger index =
      [self.tableViewModel sectionForSectionIdentifier:sectionID];
  [self.tableViewModel removeItemWithType:SettingsItemTypeESBPromo
                fromSectionWithIdentifier:sectionID
                                  atIndex:0];
  [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationFade];

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(_profile);
  tracker->NotifyEvent(
      feature_engagement::events::kInlineEnhancedSafeBrowsingPromoClosed);
  base::RecordAction(base::UserMetricsAction(
      "MobileSettingsEnhancedSafeBrowsingInlinePromoDismiss"));
  [self removeEnhancedSafeBrowsingPromoFETDataIfNeeded];
}

- (void)showSafeBrowsingSettingsMenu {
  id<SettingsCommands> handler =
      HandlerForProtocol(_browser->GetCommandDispatcher(), SettingsCommands);
  [handler showSafeBrowsingSettingsFromPromoInteraction];
  base::RecordAction(base::UserMetricsAction(
      "MobileSettingsEnhancedSafeBrowsingInlinePromoProceed"));
}

@end
