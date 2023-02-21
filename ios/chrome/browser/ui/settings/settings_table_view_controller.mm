// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"

#import <memory>

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/util.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/icons/buildflags.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/settings/about_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/bandwidth/bandwidth_management_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/content_settings/content_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/default_browser/default_browser_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/passwords_coordinator.h"
#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_coordinator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_coordinator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_utils.h"
#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/table_cell_catalog_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/voice_search_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/upgrade/upgrade_utils.h"
#import "ios/chrome/browser/voice/speech_input_locale_config.h"
#import "ios/chrome/browser/voice/voice_search_prefs.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kSyncErrorImageName = @"google_services_sync_error";
NSString* const kSyncOffImageName = @"sync_and_google_services_sync_off";
NSString* const kSyncOnImageName = @"sync_and_google_services_sync_on";
NSString* const kSettingsGoogleServicesImageName = @"settings_google_services";
NSString* const kSettingsSearchEngineImageName = @"settings_search_engine";
NSString* const kSettingsPasswordsImageName =
    @"settings_passwords";
NSString* const kSettingsAutofillCreditCardImageName =
    @"settings_payment_methods";
NSString* const kSettingsAutofillProfileImageName = @"settings_addresses";
NSString* const kSettingsVoiceSearchImageName = @"settings_voice_search";
NSString* const kSettingsSafetyCheckImageName = @"settings_safety_check";
NSString* const kSettingsPrivacyImageName = @"settings_privacy";
NSString* const kSettingsLanguageSettingsImageName =
    @"settings_language_settings";
NSString* const kSettingsContentSettingsImageName =
    @"settings_content_settings";
NSString* const kSettingsBandwidthImageName = @"settings_bandwidth";
NSString* const kSettingsBellImageName = @"settings_bell";
NSString* const kSettingsAboutChromeImageName = @"settings_about_chrome";
NSString* const kSettingsDebugImageName = @"settings_debug";
NSString* const kSettingsArticleSuggestionsImageName =
    @"settings_article_suggestions";
NSString* const kDefaultBrowserWorldImageName = @"default_browser_world";

// The size of trailing symbol icons for unsafe state.
NSInteger kTrailingSymbolImagePointSize = 22;

// Key used for storing NSUserDefault entry to keep track of the last timestamp
// we've shown the default browser blue dot promo.
NSString* const kMostRecentTimestampBlueDotPromoShownInSettingsMenu =
    @"MostRecentTimestampBlueDotPromoShownInSettingsMenu";

#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
NSString* kDevViewSourceKey = @"DevViewSource";
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

enum SyncState {
  kSyncDisabledByAdministrator,
  kSyncConsentOff,
  kSyncOff,
  kSyncEnabledWithNoSelectedTypes,
  kSyncEnabledWithError,
  kSyncEnabled,
};

SyncState GetSyncStateFromBrowserState(ChromeBrowserState* browserState) {
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  syncer::SyncService::UserActionableError errorState =
      syncService->GetUserActionableError();
  if (syncService->GetDisableReasons().Has(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    // Sync is disabled by administrator policy.
    return kSyncDisabledByAdministrator;
  } else if (!syncService->GetUserSettings()->IsFirstSetupComplete()) {
    // User has not completed Sync setup in sign-in flow.
    return kSyncConsentOff;
  } else if (!syncService->CanSyncFeatureStart()) {
    // Sync engine is off.
    return kSyncOff;
  } else if (syncService->GetUserSettings()->GetSelectedTypes().Empty()) {
    // User has deselected all sync data types.
    // With pre-MICE, the sync status should be kSyncEnabled to show the same
    // value than the sync toggle.
    return kSyncEnabledWithNoSelectedTypes;
  } else if (errorState != syncer::SyncService::UserActionableError::kNone) {
    // Sync error.
    return kSyncEnabledWithError;
  }
  return kSyncEnabled;
}

// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return CustomSettingsRootMulticolorSymbol(kGoogleIconSymbol);
#else
  return DefaultSettingsRootSymbol(@"gearshape.2");
#endif
}

}  // namespace

#pragma mark - SettingsTableViewController

@interface SettingsTableViewController () <
    BooleanObserver,
    ChromeAccountManagerServiceObserver,
    GoogleServicesSettingsCoordinatorDelegate,
    IdentityManagerObserverBridgeDelegate,
    ManageSyncSettingsCoordinatorDelegate,
    PasswordCheckObserver,
    PasswordsCoordinatorDelegate,
    PopoverLabelViewControllerDelegate,
    PrefObserverDelegate,
    PriceNotificationsCoordinatorDelegate,
    PrivacyCoordinatorDelegate,
    SafetyCheckCoordinatorDelegate,
    SettingsControllerProtocol,
    SearchEngineObserving,
    SigninPresenter,
    SigninPromoViewConsumer,
    SyncObserverModelBridge> {
  // The browser where the settings are being displayed.
  Browser* _browser;
  // The browser state for `_browser`. Never off the record.
  ChromeBrowserState* _browserState;  // weak
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
  // The item related to the switch for the show suggestions setting.
  TableViewSwitchItem* _showMemoryDebugToolsItem;
  // The item related to the safety check.
  SettingsCheckItem* _safetyCheckItem;

  // Mediator to configure the sign-in promo cell. Also used to received
  // identity update notifications.
  SigninPromoViewMediator* _signinPromoViewMediator;
  GoogleServicesSettingsCoordinator* _googleServicesSettingsCoordinator;
  ManageSyncSettingsCoordinator* _manageSyncSettingsCoordinator;

  // Price notifications coordinator.
  PriceNotificationsCoordinator* _priceNotificationsCoordinator;

  // Privacy coordinator.
  PrivacyCoordinator* _privacyCoordinator;

  // Safety Check coordinator.
  SafetyCheckCoordinator* _safetyCheckCoordinator;

  // Passwords coordinator.
  PasswordsCoordinator* _passwordsCoordinator;

  // Identity object and observer used for Account Item refresh.
  id<SystemIdentity> _identity;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;

  // PrefMember for voice locale code.
  StringPrefMember _voiceLocaleCode;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // TODO(crbug.com/662435): Refactor PrefObserverBridge so it owns the
  // PrefChangeRegistrar.
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Updatable Items.
  TableViewDetailIconItem* _voiceSearchDetailItem;
  TableViewDetailIconItem* _defaultSearchEngineItem;
  TableViewInfoButtonItem* _managedSearchEngineItem;
  TableViewDetailIconItem* _passwordsDetailItem;
  TableViewDetailIconItem* _autoFillProfileDetailItem;
  TableViewDetailIconItem* _autoFillCreditCardDetailItem;
  TableViewItem* _syncItem;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

// The item related to the switch for the show feed settings.
@property(nonatomic, strong, readonly) TableViewSwitchItem* feedSettingsItem;
// The item related to the enterprise managed show feed settings.
@property(nonatomic, strong, readonly)
    TableViewInfoButtonItem* managedFeedSettingsItem;

@property(nonatomic, readonly, weak)
    id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>
        dispatcher;

// YES if the default browser settings row is currently showing the notification
// dot.
@property(nonatomic, assign) BOOL showingDefaultBrowserNotificationDot;

// YES if the sign-in is in progress.
@property(nonatomic, assign) BOOL isSigninInProgress;

// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

@end

@implementation SettingsTableViewController
@synthesize dispatcher = _dispatcher;
@synthesize managedFeedSettingsItem = _managedFeedSettingsItem;
@synthesize feedSettingsItem = _feedSettingsItem;

#pragma mark Initialization

- (instancetype)
    initWithBrowser:(Browser*)browser
         dispatcher:
             (id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>)
                 dispatcher {
  DCHECK(browser);
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    _browserState = _browser->GetBrowserState();
    self.title = l10n_util::GetNSStringWithFixup(IDS_IOS_SETTINGS_TITLE);
    _searchEngineObserverBridge.reset(new SearchEngineObserverBridge(
        self,
        ios::TemplateURLServiceFactory::GetForBrowserState(_browserState)));
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(_browserState);
    _accountManagerService =
        ChromeAccountManagerServiceFactory::GetForBrowserState(_browserState);
    // It is expected that `identityManager` should never be nil except in
    // tests. In that case, the tests should be fixed.
    DCHECK(identityManager);
    _identityObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(identityManager, self));
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForBrowserState(_browserState);
    _syncObserverBridge.reset(new SyncObserverBridge(self, syncService));

    _showMemoryDebugToolsEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()
                   prefName:prefs::kShowMemoryDebuggingTools];
    [_showMemoryDebugToolsEnabled setObserver:self];

    AuthenticationService* authService =
        AuthenticationServiceFactory::GetForBrowserState(_browserState);
    _identity = authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
    _accountManagerServiceObserver.reset(
        new ChromeAccountManagerServiceObserverBridge(self,
                                                      _accountManagerService));

    PrefService* prefService = _browserState->GetPrefs();

    _passwordCheckManager =
        IOSChromePasswordCheckManagerFactory::GetForBrowserState(_browserState);
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

    _dispatcher = dispatcher;

    // TODO(crbug.com/764578): -loadModel should not be called from
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

  // Basics section
  [model addSectionWithIdentifier:SettingsSectionIdentifierBasics];
  [model addItem:[self passwordsDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];
  [model addItem:[self autoFillCreditCardDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];
  [model addItem:[self autoFillProfileDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierBasics];

  // Advanced Section
  [model addSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
  if (base::FeatureList::IsEnabled(kNotificationSettingsMenuItem) &&
      IsPriceNotificationsEnabled()) {
    [model addItem:[self priceNotificationsItem]
        toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
  }
  [model addItem:[self voiceSearchDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
  [model addItem:[self safetyCheckDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
  [model addItem:[self privacyDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];

  // Feed is disabled in safe mode.
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(_browser)->GetSceneState();
  BOOL isSafeMode = [sceneState.appState resumingFromSafeMode];

  if (!IsFeedAblationEnabled() && !isSafeMode &&
      IsContentSuggestionsForSupervisedUserEnabled(_browserState->GetPrefs())) {
    if ([_contentSuggestionPolicyEnabled value]) {
      [model addItem:self.feedSettingsItem
          toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];

    } else {
      [model addItem:self.managedFeedSettingsItem
          toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
    }
  }
  [model addItem:[self languageSettingsDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
  [model addItem:[self contentSettingsDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];
  [model addItem:[self bandwidthManagementDetailItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAdvanced];

  // Info Section
  [model addSectionWithIdentifier:SettingsSectionIdentifierInfo];
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
}

// Helper method to update the Discover Section cells when called.
- (void)updateDiscoverSection {
  // Do not use self to access _managedFeedSettingsItem, as it is lazy loaded
  // and will create a new item and the following will always be true.
  if (_managedFeedSettingsItem) {
    DCHECK(!_feedSettingsItem);
    self.managedFeedSettingsItem.text = [self feedItemTitle];
  } else {
    DCHECK(_feedSettingsItem);
    self.feedSettingsItem.text = [self feedItemTitle];
  }
}

// Adds the identity promo to promote the sign-in or sync state.
- (void)addPromoToSigninSection {
  TableViewItem* item = nil;

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(_browserState);
  const AuthenticationService::ServiceStatus authServiceStatus =
      authService->GetServiceStatus();
  // If sign-in is disabled by policy there should not be a sign-in promo.
  if ((authServiceStatus ==
       AuthenticationService::ServiceStatus::SigninDisabledByPolicy) ||
      ([self isSyncDisabledByPolicy] &&
       !authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin))) {
    item = [self signinDisabledByPolicyTextItem];
  } else if (self.shouldDisplaySyncPromo) {
    // Create the sign-in promo mediator if it doesn't exist.
    if (!_signinPromoViewMediator) {
      _signinPromoViewMediator = [[SigninPromoViewMediator alloc]
          initWithAccountManagerService:self.accountManagerService
                            authService:AuthenticationServiceFactory::
                                            GetForBrowserState(_browserState)
                            prefService:_browserState->GetPrefs()
                            accessPoint:signin_metrics::AccessPoint::
                                            ACCESS_POINT_SETTINGS
                              presenter:self];
      _signinPromoViewMediator.consumer = self;
    }
    TableViewSigninPromoItem* signinPromoItem =
        [[TableViewSigninPromoItem alloc]
            initWithType:SettingsItemTypeSigninPromo];
    signinPromoItem.text =
        l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_SETTINGS_WITH_UNITY);
    signinPromoItem.configurator =
        [_signinPromoViewMediator createConfigurator];
    signinPromoItem.delegate = _signinPromoViewMediator;
    [_signinPromoViewMediator signinPromoViewIsVisible];

    item = signinPromoItem;
  } else if ((authServiceStatus ==
                  AuthenticationService::ServiceStatus::SigninForcedByPolicy ||
              authServiceStatus ==
                  AuthenticationService::ServiceStatus::SigninAllowed) &&
             !authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    item = [self accountSignInItem];
    [_signinPromoViewMediator disconnect];
    _signinPromoViewMediator = nil;
  } else {
    [self.tableViewModel
        removeSectionWithIdentifier:SettingsSectionIdentifierSignIn];
    [_signinPromoViewMediator disconnect];
    _signinPromoViewMediator = nil;

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
      AuthenticationServiceFactory::GetForBrowserState(_browserState);
  if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    // Account profile item.
    [model addItem:[self accountCellItem]
        toSectionWithIdentifier:SettingsSectionIdentifierAccount];
    _hasRecordedSigninImpression = NO;
  }

  // Sync item.
  if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    [model addItem:[self syncItem]
        toSectionWithIdentifier:SettingsSectionIdentifierAccount];
  }
  // Google Services item.
  [model addItem:[self googleServicesCellItem]
      toSectionWithIdentifier:SettingsSectionIdentifierAccount];
}

#pragma mark - Properties

// Returns YES if the Sync service is available and all promos have not been
// previously closed or seen too many times by a single user account.
- (BOOL)shouldDisplaySyncPromo {
  if ([self isSyncDisabledByPolicy])
    return false;

  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(_browserState);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(_browserState);
  return [SigninPromoViewMediator
             shouldDisplaySigninPromoViewWithAccessPoint:
                 signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS
                                   authenticationService:authenticationService
                                             prefService:_browserState
                                                             ->GetPrefs()] &&
         !syncService->GetUserSettings()->IsFirstSetupComplete();
}

#pragma mark - Model Items

- (TableViewItem*)accountSignInItem {
  AccountSignInItem* signInTextItem =
      [[AccountSignInItem alloc] initWithType:SettingsItemTypeSignInButton];
  signInTextItem.accessibilityIdentifier = kSettingsSignInCellId;
  PrefService* prefService = _browserState->GetPrefs();
  if (!HasManagedSyncDataType(prefService)) {
    signInTextItem.detailText =
        l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SUBTITLE);
  } else {
    signInTextItem.detailText = l10n_util::GetNSString(
        IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SUBTITLE_SYNC_MANAGED);
  }
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
  if (UseSymbols()) {
    return [self detailItemWithType:SettingsItemTypeGoogleServices
                               text:l10n_util::GetNSString(
                                        IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE)
                         detailText:nil
                             symbol:GetBrandedGoogleServicesSymbol()
              symbolBackgroundColor:nil
            accessibilityIdentifier:kSettingsGoogleServicesCellId];
  }
  return [self detailItemWithType:SettingsItemTypeGoogleServices
                             text:l10n_util::GetNSString(
                                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE)
                       detailText:nil
                    iconImageName:kSettingsGoogleServicesImageName
          accessibilityIdentifier:kSettingsGoogleServicesCellId];
}

- (TableViewItem*)syncDisabledByPolicyItem {
  UIImage* image = nil;
  UIColor* backgroundColor = nil;
  if (UseSymbols()) {
    image = CustomSettingsRootSymbol(kSyncDisabledSymbol);
    backgroundColor = [UIColor colorNamed:kGrey400Color];
  } else {
    image = [UIImage imageNamed:kSyncOffImageName];
  }
  return [self infoButtonWithType:SettingsItemTypeGoogleSync
                             text:l10n_util::GetNSString(
                                      IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE)
                           status:l10n_util::GetNSString(IDS_IOS_SETTING_OFF)
                            image:image
                  imageBackground:backgroundColor
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

  TableViewDetailIconItem* syncItem = nil;
  if (UseSymbols()) {
    syncItem = [self detailItemWithType:SettingsItemTypeGoogleSync
                                   text:l10n_util::GetNSString(
                                            IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE)
                             detailText:nil
                                 symbol:nil
                  symbolBackgroundColor:nil
                accessibilityIdentifier:kSettingsGoogleSyncAndServicesCellId];
  } else {
    syncItem = [self detailItemWithType:SettingsItemTypeGoogleSync
                                   text:l10n_util::GetNSString(
                                            IDS_IOS_GOOGLE_SYNC_SETTINGS_TITLE)
                             detailText:nil
                          iconImageName:nil
                accessibilityIdentifier:kSettingsGoogleSyncAndServicesCellId];
  }
  [self updateSyncItem:syncItem];
  _syncItem = syncItem;

  return _syncItem;
}

- (TableViewItem*)defaultBrowserCellItem {
  TableViewDetailIconItem* defaultBrowser = [[TableViewDetailIconItem alloc]
      initWithType:SettingsItemTypeDefaultBrowser];
  defaultBrowser.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  defaultBrowser.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_DEFAULT_BROWSER);

  if (UseSymbols()) {
    if (@available(iOS 15, *)) {
      defaultBrowser.iconImage =
          DefaultSettingsRootSymbol(kDefaultBrowserSymbol);
    } else {
      defaultBrowser.iconImage =
          DefaultSettingsRootSymbol(kDefaultBrowseriOS14Symbol);
    }
    defaultBrowser.iconBackgroundColor = [UIColor colorNamed:kPurple500Color];
    defaultBrowser.iconTintColor = UIColor.whiteColor;
    defaultBrowser.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
  } else {
    defaultBrowser.iconImage =
        [UIImage imageNamed:kDefaultBrowserWorldImageName];
  }

  [self maybeActivateDefaultBrowserBlueDotPromo:defaultBrowser];

  return defaultBrowser;
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
          ios::TemplateURLServiceFactory::GetForBrowserState(_browserState)));

  if (UseSymbols()) {
    _defaultSearchEngineItem =
        [self detailItemWithType:SettingsItemTypeSearchEngine
                               text:l10n_util::GetNSString(
                                        IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)
                         detailText:defaultSearchEngineName
                             symbol:DefaultSettingsRootSymbol(kSearchSymbol)
              symbolBackgroundColor:[UIColor colorNamed:kPurple500Color]
            accessibilityIdentifier:kSettingsSearchEngineCellId];
  } else {
    _defaultSearchEngineItem =
        [self detailItemWithType:SettingsItemTypeSearchEngine
                               text:l10n_util::GetNSString(
                                        IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)
                         detailText:defaultSearchEngineName
                      iconImageName:kSettingsSearchEngineImageName
            accessibilityIdentifier:kSettingsSearchEngineCellId];
  }

  return _defaultSearchEngineItem;
}

- (TableViewInfoButtonItem*)managedSearchEngineItem {
  UIImage* image = nil;
  UIColor* backgroundColor = nil;
  if (UseSymbols()) {
    image = DefaultSettingsRootSymbol(kSearchSymbol);
    backgroundColor = [UIColor colorNamed:kPurple500Color];
  } else {
    image = [UIImage imageNamed:kSettingsSearchEngineImageName];
  }

  _managedSearchEngineItem =
      [self infoButtonWithType:SettingsItemTypeManagedDefaultSearchEngine
                             text:l10n_util::GetNSString(
                                      IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)
                           status:[self managedSearchEngineDetailText]
                            image:image
                  imageBackground:backgroundColor
                accessibilityHint:
                    l10n_util::GetNSString(
                        IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT)
          accessibilityIdentifier:kSettingsManagedSearchEngineCellId];

  return _managedSearchEngineItem;
}

- (TableViewItem*)passwordsDetailItem {
  BOOL passwordsEnabled = _browserState->GetPrefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService);

  NSString* passwordsDetail = passwordsEnabled
                                  ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

  NSString* passwordsSectionTitle =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);

  if (UseSymbols()) {
    _passwordsDetailItem =
        [self detailItemWithType:SettingsItemTypePasswords
                               text:passwordsSectionTitle
                         detailText:passwordsDetail
                             symbol:CustomSettingsRootSymbol(kPasswordSymbol)
              symbolBackgroundColor:[UIColor colorNamed:kYellow500Color]
            accessibilityIdentifier:kSettingsPasswordsCellId];
  } else {
    _passwordsDetailItem = [self detailItemWithType:SettingsItemTypePasswords
                                               text:passwordsSectionTitle
                                         detailText:passwordsDetail
                                      iconImageName:kSettingsPasswordsImageName
                            accessibilityIdentifier:kSettingsPasswordsCellId];
  }

  return _passwordsDetailItem;
}

- (TableViewItem*)autoFillCreditCardDetailItem {
  BOOL autofillCreditCardEnabled =
      autofill::prefs::IsAutofillCreditCardEnabled(_browserState->GetPrefs());
  NSString* detailText = autofillCreditCardEnabled
                             ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                             : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

  if (UseSymbols()) {
    _autoFillCreditCardDetailItem =
        [self detailItemWithType:SettingsItemTypeAutofillCreditCard
                               text:l10n_util::GetNSString(
                                        IDS_AUTOFILL_PAYMENT_METHODS)
                         detailText:detailText
                             symbol:DefaultSettingsRootSymbol(kCreditCardSymbol)
              symbolBackgroundColor:[UIColor colorNamed:kYellow500Color]
            accessibilityIdentifier:kSettingsPaymentMethodsCellId];
  } else {
    _autoFillCreditCardDetailItem =
        [self detailItemWithType:SettingsItemTypeAutofillCreditCard
                               text:l10n_util::GetNSString(
                                        IDS_AUTOFILL_PAYMENT_METHODS)
                         detailText:detailText
                      iconImageName:kSettingsAutofillCreditCardImageName
            accessibilityIdentifier:kSettingsPaymentMethodsCellId];
  }

  return _autoFillCreditCardDetailItem;
}

- (TableViewItem*)autoFillProfileDetailItem {
  BOOL autofillProfileEnabled =
      autofill::prefs::IsAutofillProfileEnabled(_browserState->GetPrefs());
  NSString* detailText = autofillProfileEnabled
                             ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                             : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

  if (UseSymbols()) {
    _autoFillProfileDetailItem =
        [self detailItemWithType:SettingsItemTypeAutofillProfile
                               text:l10n_util::GetNSString(
                                        IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE)
                         detailText:detailText
                             symbol:CustomSettingsRootSymbol(kLocationSymbol)
              symbolBackgroundColor:[UIColor colorNamed:kYellow500Color]
            accessibilityIdentifier:kSettingsAddressesAndMoreCellId];
  } else {
    _autoFillProfileDetailItem =
        [self detailItemWithType:SettingsItemTypeAutofillProfile
                               text:l10n_util::GetNSString(
                                        IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE)
                         detailText:detailText
                      iconImageName:kSettingsAutofillProfileImageName
            accessibilityIdentifier:kSettingsAddressesAndMoreCellId];
  }
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

  if (UseSymbols()) {
    _voiceSearchDetailItem =
        [self detailItemWithType:SettingsItemTypeVoiceSearch
                               text:l10n_util::GetNSString(
                                        IDS_IOS_VOICE_SEARCH_SETTING_TITLE)
                         detailText:languageName
                             symbol:DefaultSettingsRootSymbol(kMicrophoneSymbol)
              symbolBackgroundColor:[UIColor colorNamed:kGreen500Color]
            accessibilityIdentifier:kSettingsVoiceSearchCellId];
  } else {
    _voiceSearchDetailItem =
        [self detailItemWithType:SettingsItemTypeVoiceSearch
                               text:l10n_util::GetNSString(
                                        IDS_IOS_VOICE_SEARCH_SETTING_TITLE)
                         detailText:languageName
                      iconImageName:kSettingsVoiceSearchImageName
            accessibilityIdentifier:kSettingsVoiceSearchCellId];
  }

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
  if (UseSymbols()) {
    // TODO(crbug.com/1315544): This is probably not the right icon (same as the
    // default one).
    _safetyCheckItem.leadingIcon = CustomSettingsRootSymbol(kSafetyCheckSymbol);
    _safetyCheckItem.leadingIconBackgroundColor =
        [UIColor colorNamed:kBlue500Color];
    _safetyCheckItem.leadingIconTintColor = UIColor.whiteColor;
    _safetyCheckItem.leadingIconCornerRadius =
        kColorfulBackgroundSymbolCornerRadius;
  } else {
    UIImage* safetyCheckIcon =
        [UIImage imageNamed:kSettingsSafetyCheckImageName];
    _safetyCheckItem.leadingIcon = safetyCheckIcon;
  }
  // Check if an issue state should be shown for updates.
  if (!IsAppUpToDate() && PreviousSafetyCheckIssueFound()) {
    UIImage* unSafeIconImage =
        UseSymbols()
            ? DefaultSymbolTemplateWithPointSize(kWarningFillSymbol,
                                                 kTrailingSymbolImagePointSize)
            : [[UIImage imageNamed:@"settings_unsafe_state"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _safetyCheckItem.trailingImage = unSafeIconImage;
    _safetyCheckItem.trailingImageTintColor = [UIColor colorNamed:kRedColor];
  }

  return _safetyCheckItem;
}

- (TableViewItem*)priceNotificationsItem {
  NSString* title = l10n_util::GetNSString(IDS_IOS_PRICE_NOTIFICATIONS_TITLE);

  // TODO(crbug.com/1363175): Replace kSettingsPrivacyImageName.
  if (UseSymbols()) {
    return [self detailItemWithType:SettingsItemTypePriceNotifications
                               text:title
                         detailText:nil
                             symbol:DefaultSettingsRootSymbol(kBellSymbol)
              symbolBackgroundColor:[UIColor colorNamed:kPink500Color]
            accessibilityIdentifier:kSettingsPriceNotificationsId];
  }

  return [self detailItemWithType:SettingsItemTypePriceNotifications
                             text:title
                       detailText:nil
                    iconImageName:kSettingsBellImageName
          accessibilityIdentifier:kSettingsPriceNotificationsId];
}

- (TableViewItem*)privacyDetailItem {
  NSString* title = nil;
  title = l10n_util::GetNSString(IDS_IOS_SETTINGS_PRIVACY_TITLE);

  if (UseSymbols()) {
    return [self detailItemWithType:SettingsItemTypePrivacy
                               text:title
                         detailText:nil
                             symbol:CustomSettingsRootSymbol(kPrivacySymbol)
              symbolBackgroundColor:[UIColor colorNamed:kBlue500Color]
            accessibilityIdentifier:kSettingsPrivacyCellId];
  }

  return [self detailItemWithType:SettingsItemTypePrivacy
                             text:title
                       detailText:nil
                    iconImageName:kSettingsPrivacyImageName
          accessibilityIdentifier:kSettingsPrivacyCellId];
}

- (TableViewSwitchItem*)feedSettingsItem {
  if (!_feedSettingsItem) {
    NSString* settingTitle = [self feedItemTitle];

    if (UseSymbols()) {
      _feedSettingsItem =
          [self switchItemWithType:SettingsItemTypeArticlesForYou
                                title:settingTitle
                               symbol:DefaultSettingsRootSymbol(kDiscoverSymbol)
                symbolBackgroundColor:[UIColor colorNamed:kOrange500Color]
              accessibilityIdentifier:kSettingsArticleSuggestionsCellId];
    } else {
      _feedSettingsItem =
          [self switchItemWithType:SettingsItemTypeArticlesForYou
                                title:settingTitle
                        iconImageName:kSettingsArticleSuggestionsImageName
              accessibilityIdentifier:kSettingsArticleSuggestionsCellId];
    }
    _feedSettingsItem.on = [_articlesEnabled value];
  }
  return _feedSettingsItem;
}

- (TableViewInfoButtonItem*)managedFeedSettingsItem {
  if (!_managedFeedSettingsItem) {
    UIImage* image = nil;
    UIColor* backgroundColor = nil;
    if (UseSymbols()) {
      image = DefaultSettingsRootSymbol(kDiscoverSymbol);
      backgroundColor = [UIColor colorNamed:kOrange500Color];
    } else {
      _managedFeedSettingsItem.iconImage =
          [UIImage imageNamed:kSettingsArticleSuggestionsImageName];
    }

    _managedFeedSettingsItem =
        [self infoButtonWithType:SettingsItemTypeManagedArticlesForYou
                               text:[self feedItemTitle]
                             status:l10n_util::GetNSString(IDS_IOS_SETTING_OFF)
                              image:image
                    imageBackground:backgroundColor
                  accessibilityHint:
                      l10n_util::GetNSString(
                          IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT)
            accessibilityIdentifier:kSettingsArticleSuggestionsCellId];
  }

  return _managedFeedSettingsItem;
}

- (TableViewItem*)languageSettingsDetailItem {
  if (UseSymbols()) {
    return [self detailItemWithType:SettingsItemTypeLanguageSettings
                               text:l10n_util::GetNSString(
                                        IDS_IOS_LANGUAGE_SETTINGS_TITLE)
                         detailText:nil
                             symbol:CustomSettingsRootSymbol(kLanguageSymbol)
              symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
            accessibilityIdentifier:kSettingsLanguagesCellId];
  }

  return [self detailItemWithType:SettingsItemTypeLanguageSettings
                             text:l10n_util::GetNSString(
                                      IDS_IOS_LANGUAGE_SETTINGS_TITLE)
                       detailText:nil
                    iconImageName:kSettingsLanguageSettingsImageName
          accessibilityIdentifier:kSettingsLanguagesCellId];
}

- (TableViewItem*)contentSettingsDetailItem {
  if (UseSymbols()) {
    return [self
             detailItemWithType:SettingsItemTypeContentSettings
                           text:l10n_util::GetNSString(
                                    IDS_IOS_CONTENT_SETTINGS_TITLE)
                     detailText:nil
                         symbol:DefaultSettingsRootSymbol(kSettingsFilledSymbol)
          symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
        accessibilityIdentifier:kSettingsContentSettingsCellId];
  }

  return [self detailItemWithType:SettingsItemTypeContentSettings
                             text:l10n_util::GetNSString(
                                      IDS_IOS_CONTENT_SETTINGS_TITLE)
                       detailText:nil
                    iconImageName:kSettingsContentSettingsImageName
          accessibilityIdentifier:kSettingsContentSettingsCellId];
}

- (TableViewItem*)bandwidthManagementDetailItem {
  if (UseSymbols()) {
    return [self detailItemWithType:SettingsItemTypeBandwidth
                               text:l10n_util::GetNSString(
                                        IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS)
                         detailText:nil
                             symbol:DefaultSettingsRootSymbol(kWifiSymbol)
              symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
            accessibilityIdentifier:kSettingsBandwidthCellId];
  } else {
    return [self detailItemWithType:SettingsItemTypeBandwidth
                               text:l10n_util::GetNSString(
                                        IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS)
                         detailText:nil
                      iconImageName:kSettingsBandwidthImageName
            accessibilityIdentifier:kSettingsBandwidthCellId];
  }
}

- (TableViewItem*)aboutChromeDetailItem {
  if (UseSymbols()) {
    return [self detailItemWithType:SettingsItemTypeAboutChrome
                               text:l10n_util::GetNSString(IDS_IOS_PRODUCT_NAME)
                         detailText:nil
                             symbol:DefaultSettingsRootSymbol(kInfoCircleSymbol)
              symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
            accessibilityIdentifier:kSettingsAboutCellId];
  }

  return [self detailItemWithType:SettingsItemTypeAboutChrome
                             text:l10n_util::GetNSString(IDS_IOS_PRODUCT_NAME)
                       detailText:nil
                    iconImageName:kSettingsAboutChromeImageName
          accessibilityIdentifier:kSettingsAboutCellId];
}

- (TableViewSwitchItem*)showMemoryDebugSwitchItem {
  TableViewSwitchItem* showMemoryDebugSwitchItem;
  if (UseSymbols()) {
    showMemoryDebugSwitchItem =
        [self switchItemWithType:SettingsItemTypeMemoryDebugging
                              title:@"Show memory debug tools"
                             symbol:DefaultSettingsRootSymbol(@"memorychip")
              symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
            accessibilityIdentifier:nil];
  } else {
    showMemoryDebugSwitchItem =
        [self switchItemWithType:SettingsItemTypeMemoryDebugging
                              title:@"Show memory debug tools"
                      iconImageName:kSettingsDebugImageName
            accessibilityIdentifier:nil];
  }
  showMemoryDebugSwitchItem.on = [_showMemoryDebugToolsEnabled value];

  return showMemoryDebugSwitchItem;
}

#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

- (TableViewSwitchItem*)viewSourceSwitchItem {
  TableViewSwitchItem* viewSourceItem = nil;
  if (UseSymbols()) {
    UIImage* image;
    if (@available(iOS 16, *)) {
      image = DefaultSettingsRootSymbol(@"keyboard.badge.eye");
    } else {
      image = DefaultSettingsRootSymbol(@"keyboard");
    }
    viewSourceItem = [self switchItemWithType:SettingsItemTypeViewSource
                                        title:@"View source menu"
                                       symbol:image
                        symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
                      accessibilityIdentifier:nil];

  } else {
    viewSourceItem = [self switchItemWithType:SettingsItemTypeViewSource
                                        title:@"View source menu"
                                iconImageName:kSettingsDebugImageName
                      accessibilityIdentifier:nil];
  }
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  viewSourceItem.on = [defaults boolForKey:kDevViewSourceKey];
  return viewSourceItem;
}

- (TableViewDetailIconItem*)tableViewCatalogDetailItem {
  if (UseSymbols()) {
    return [self detailItemWithType:SettingsItemTypeTableCellCatalog
                               text:@"TableView Cell Catalog"
                         detailText:nil
                             symbol:DefaultSettingsRootSymbol(@"cart")
              symbolBackgroundColor:[UIColor colorNamed:kGrey400Color]
            accessibilityIdentifier:nil];
  }
  return [self detailItemWithType:SettingsItemTypeTableCellCatalog
                             text:@"TableView Cell Catalog"
                       detailText:nil
                    iconImageName:kSettingsDebugImageName
          accessibilityIdentifier:nil];
}
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

#pragma mark Item Constructors

- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                          text:(NSString*)text
                                    detailText:(NSString*)detailText
                                 iconImageName:(NSString*)iconImageName
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  DCHECK(!UseSymbols());
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.detailText = detailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  if (iconImageName)
    detailItem.iconImage = [UIImage imageNamed:iconImageName];
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;
  return detailItem;
}

- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                          text:(NSString*)text
                                    detailText:(NSString*)detailText
                                        symbol:(UIImage*)symbol
                         symbolBackgroundColor:(UIColor*)backgroundColor
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  DCHECK(UseSymbols());
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
                             iconImageName:(NSString*)iconImageName
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  DCHECK(!UseSymbols());
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:type];
  switchItem.text = title;
  switchItem.iconImage = [UIImage imageNamed:iconImageName];
  switchItem.accessibilityIdentifier = accessibilityIdentifier;

  return switchItem;
}

- (TableViewSwitchItem*)switchItemWithType:(NSInteger)type
                                     title:(NSString*)title
                                    symbol:(UIImage*)symbol
                     symbolBackgroundColor:(UIColor*)backgroundColor
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  DCHECK(UseSymbols());
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
    if (UseSymbols()) {
      DCHECK(imageBackground);
      infoButton.iconBackgroundColor = imageBackground;
      infoButton.iconTintColor = UIColor.whiteColor;
      infoButton.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
    }
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

  if ([cell isKindOfClass:[TableViewDetailIconCell class]]) {
    TableViewDetailIconCell* detailCell =
        base::mac::ObjCCastStrict<TableViewDetailIconCell>(cell);
    if (itemType == SettingsItemTypePasswords) {
      scoped_refptr<password_manager::PasswordStoreInterface> passwordStore =
          IOSChromePasswordStoreFactory::GetForBrowserState(
              _browserState, ServiceAccessType::EXPLICIT_ACCESS);
      if (!passwordStore) {
        // The password store factory returns a NULL password store if something
        // goes wrong during the password store initialization. Disable the save
        // passwords cell in this case.
        LOG(ERROR) << "Save passwords cell was disabled as the password store"
                      " cannot be created.";
        [detailCell setUserInteractionEnabled:NO];
        detailCell.textLabel.textColor =
            [UIColor colorNamed:kTextSecondaryColor];
        return cell;
      }
    }

    [detailCell setUserInteractionEnabled:YES];
    detailCell.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }

  switch (itemType) {
    case SettingsItemTypeMemoryDebugging: {
      TableViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(memorySwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case SettingsItemTypeArticlesForYou: {
      TableViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(articlesForYouSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case SettingsItemTypeViewSource: {
#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
      TableViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(viewSourceSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
#else
      NOTREACHED();
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
      break;
    }
    case SettingsItemTypeManagedDefaultSearchEngine: {
      TableViewInfoButtonCell* managedCell =
          base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
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
          base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapSigninDisabledInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case SettingsItemTypeManagedArticlesForYou: {
      TableViewInfoButtonCell* managedCell =
          base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
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
          base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
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
      [self showSignInWithIdentity:nil
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_NO_SIGNIN_PROMO
                        completion:nil];
      break;
    case SettingsItemTypeAccount: {
      base::RecordAction(base::UserMetricsAction("Settings.MyAccount"));
      AccountsTableViewController* accountsTableViewController =
          [[AccountsTableViewController alloc] initWithBrowser:_browser
                                     closeSettingsOnAddAccount:NO];
      accountsTableViewController.applicationCommandsHandler =
          self.applicationCommandsHandler;
      controller = accountsTableViewController;
      break;
    }
    case SettingsItemTypeGoogleServices:
      base::RecordAction(base::UserMetricsAction("Settings.GoogleServices"));
      [self showGoogleServices];
      break;
    case SettingsItemTypeGoogleSync: {
      base::RecordAction(base::UserMetricsAction("Settings.Sync"));
      switch (GetSyncStateFromBrowserState(_browserState)) {
        case kSyncConsentOff: {
          [self showSignInWithIdentity:nil
                           promoAction:signin_metrics::PromoAction::
                                           PROMO_ACTION_NO_SIGNIN_PROMO
                            completion:nil];
          break;
        }
        case kSyncOff: {
          [self showGoogleSync];
          break;
        }
        case kSyncEnabled:
        case kSyncEnabledWithError:
        case kSyncEnabledWithNoSelectedTypes: {
          [self showGoogleSync];
          break;
        }
        case kSyncDisabledByAdministrator:
          break;
      }
      break;
    }
    case SettingsItemTypeDefaultBrowser: {
      base::RecordAction(
          base::UserMetricsAction("Settings.ShowDefaultBrowser"));

      if (self.showingDefaultBrowserNotificationDot) {
        feature_engagement::Tracker* tracker =
            feature_engagement::TrackerFactory::GetForBrowserState(
                _browserState);
        if (tracker) {
          tracker->NotifyEvent(
              feature_engagement::events::kBlueDotPromoSettingsDismissed);
        }
        [self reloadData];
      }

      controller = [[DefaultBrowserSettingsTableViewController alloc] init];
      break;
    }
    case SettingsItemTypeSearchEngine:
      base::RecordAction(base::UserMetricsAction("EditSearchEngines"));
      controller = [[SearchEngineTableViewController alloc]
          initWithBrowserState:_browserState];
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
    case SettingsItemTypePriceNotifications:
      DCHECK(IsPriceNotificationsEnabled());
      [self showPriceNotifications];
      break;
    case SettingsItemTypeVoiceSearch:
      base::RecordAction(base::UserMetricsAction("Settings.VoiceSearch"));
      controller = [[VoiceSearchTableViewController alloc]
          initWithPrefs:_browserState->GetPrefs()];
      break;
    case SettingsItemTypeSafetyCheck:
      [self showSafetyCheck];
      break;
    case SettingsItemTypePrivacy:
      base::RecordAction(base::UserMetricsAction("Settings.Privacy"));
      [self showPrivacy];
      break;
    case SettingsItemTypeLanguageSettings: {
      base::RecordAction(base::UserMetricsAction("Settings.Language"));
      LanguageSettingsMediator* mediator =
          [[LanguageSettingsMediator alloc] initWithBrowserState:_browserState];
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
    case SettingsItemTypeBandwidth:
      base::RecordAction(base::UserMetricsAction("Settings.Bandwidth"));
      controller = [[BandwidthManagementTableViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case SettingsItemTypeAboutChrome: {
      base::RecordAction(base::UserMetricsAction("AboutChrome"));
      AboutChromeTableViewController* aboutChromeTableViewController =
          [[AboutChromeTableViewController alloc] init];
      aboutChromeTableViewController.applicationCommandsHandler =
          self.applicationCommandsHandler;
      aboutChromeTableViewController.snackbarCommandsHandler =
          self.snackbarCommandsHandler;
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
    default:
      break;
  }

  if (controller) {
    controller.dispatcher = self.dispatcher;
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
      base::mac::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [_showMemoryDebugToolsEnabled setValue:newSwitchValue];
}

- (void)articlesForYouSwitchToggled:(UISwitch*)sender {
  NSIndexPath* switchPath = [self.tableViewModel
      indexPathForItemType:SettingsItemTypeArticlesForYou
         sectionIdentifier:SettingsSectionIdentifierAdvanced];

  TableViewSwitchItem* switchItem =
      base::mac::ObjCCastStrict<TableViewSwitchItem>(
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
      base::mac::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [self setBooleanNSUserDefaultsValue:newSwitchValue forKey:kDevViewSourceKey];
}
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

#pragma mark - Private methods

// Returns true if sync is disabled by policy.
- (bool)isSyncDisabledByPolicy {
  return GetSyncStateFromBrowserState(_browserState) ==
         kSyncDisabledByAdministrator;
}

- (void)showGoogleServices {
  DCHECK(!_googleServicesSettingsCoordinator);
  _googleServicesSettingsCoordinator =
      [[GoogleServicesSettingsCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:_browser];
  _googleServicesSettingsCoordinator.delegate = self;
  [_googleServicesSettingsCoordinator start];
}

- (void)showGoogleSync {
  DCHECK(!_manageSyncSettingsCoordinator);
  _manageSyncSettingsCoordinator = [[ManageSyncSettingsCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  _manageSyncSettingsCoordinator.delegate = self;
  [_manageSyncSettingsCoordinator start];
}

- (void)showPasswords {
  DCHECK(!_passwordsCoordinator);
  _passwordsCoordinator = [[PasswordsCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  _passwordsCoordinator.delegate = self;
  [_passwordsCoordinator start];
}

// Shows Safety Check Screen.
- (void)showSafetyCheck {
  DCHECK(!_safetyCheckCoordinator);
  _safetyCheckCoordinator = [[SafetyCheckCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  _safetyCheckCoordinator.delegate = self;
  [_safetyCheckCoordinator start];
}

// Checks if there are any remaining password issues that are not muted from the
// last time password check was run.
- (BOOL)hasPasswordIssuesRemaining {
  return !_passwordCheckManager->GetInsecureCredentials().empty();
}

// Displays a red issue state on `_safetyCheckItem` if there is a reamining
// issue for any of the checks.
- (void)setSafetyCheckIssueStateUnsafe:(BOOL)isUnsafe {
  if (isUnsafe && PreviousSafetyCheckIssueFound()) {
    UIImage* unSafeIconImage =
        UseSymbols()
            ? DefaultSymbolTemplateWithPointSize(kWarningFillSymbol,
                                                 kTrailingSymbolImagePointSize)
            : [[UIImage imageNamed:@"settings_unsafe_state"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _safetyCheckItem.trailingImage = unSafeIconImage;
    _safetyCheckItem.trailingImageTintColor = [UIColor colorNamed:kRedColor];
  } else {
    _safetyCheckItem.trailingImage = nil;
    _safetyCheckItem.trailingImageTintColor = nil;
  }
  [self reconfigureCellsForItems:@[ _safetyCheckItem ]];
}

// Shows Price Notifications screen.
- (void)showPriceNotifications {
  DCHECK(!_priceNotificationsCoordinator);
  DCHECK(self.navigationController);
  _priceNotificationsCoordinator = [[PriceNotificationsCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser];
  _priceNotificationsCoordinator.delegate = self;
  [_priceNotificationsCoordinator start];
}

// Shows Privacy screen.
- (void)showPrivacy {
  DCHECK(!_privacyCoordinator);
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
      AuthenticationServiceFactory::GetForBrowserState(_browserState);
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
      base::mac::ObjCCast<TableViewAccountItem>(
          [self.tableViewModel itemAtIndexPath:accountCellIndexPath]);
  if (identityAccountItem) {
    [self updateIdentityAccountItem:identityAccountItem];
    [self reconfigureCellsForItems:@[ identityAccountItem ]];
  }
}

// Updates the Sync item to display the right icon and status message in the
// cell.
- (void)updateSyncItem:(TableViewDetailIconItem*)googleSyncItem {
  switch (GetSyncStateFromBrowserState(_browserState)) {
    case kSyncConsentOff: {
      googleSyncItem.detailText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
      if (UseSymbols()) {
        googleSyncItem.iconImage =
            CustomSettingsRootSymbol(kSyncDisabledSymbol);
        googleSyncItem.iconBackgroundColor = [UIColor colorNamed:kGrey400Color];
        googleSyncItem.iconTintColor = UIColor.whiteColor;
        googleSyncItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;

      } else {
        googleSyncItem.iconImage = [UIImage imageNamed:kSyncOffImageName];
      }
      break;
    }
    case kSyncOff:
    case kSyncEnabledWithNoSelectedTypes: {
      googleSyncItem.detailText = nil;
      if (UseSymbols()) {
        googleSyncItem.iconImage =
            CustomSettingsRootSymbol(kSyncDisabledSymbol);
        googleSyncItem.iconBackgroundColor = [UIColor colorNamed:kGrey400Color];
        googleSyncItem.iconTintColor = UIColor.whiteColor;
        googleSyncItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;

      } else {
        googleSyncItem.iconImage = [UIImage imageNamed:kSyncOffImageName];
      }
      break;
    }
    case kSyncEnabledWithError: {
      syncer::SyncService* syncService =
          SyncServiceFactory::GetForBrowserState(_browserState);
      googleSyncItem.detailText =
          GetSyncErrorDescriptionForSyncService(syncService);
      if (UseSymbols()) {
        googleSyncItem.iconImage = DefaultSettingsRootSymbol(kSyncErrorSymbol);
        googleSyncItem.iconBackgroundColor = [UIColor colorNamed:kRed500Color];
        googleSyncItem.iconTintColor = UIColor.whiteColor;
        googleSyncItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
      } else {
        googleSyncItem.iconImage = [UIImage imageNamed:kSyncErrorImageName];
      }
      // Return a vertical layout of title / subtitle in the case of a sync
      // error.
      googleSyncItem.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
      return;
    }
    case kSyncEnabled: {
      googleSyncItem.detailText = l10n_util::GetNSString(IDS_IOS_SETTING_ON);

      if (UseSymbols()) {
        googleSyncItem.iconImage =
            DefaultSettingsRootSymbol(kSyncEnabledSymbol);
        googleSyncItem.iconBackgroundColor =
            [UIColor colorNamed:kGreen500Color];
        googleSyncItem.iconTintColor = UIColor.whiteColor;
        googleSyncItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
      } else {
        googleSyncItem.iconImage = [UIImage imageNamed:kSyncOnImageName];
      }
      break;
    }
    case kSyncDisabledByAdministrator:
      // Nothing to update.
      break;
  }
  // Needed to update the item text layout in the case that it was previously
  // set to UILayoutConstraintAxisVertical due to a sync error.
  googleSyncItem.textLayoutConstraintAxis = UILayoutConstraintAxisHorizontal;
}

// Check if the default search engine is managed by policy.
- (BOOL)isDefaultSearchEngineManagedByPolicy {
  const base::Value::Dict& dict = _browserState->GetPrefs()->GetDict(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);

  if (dict.FindBoolByDottedPath(DefaultSearchManager::kDisabledByPolicy) ||
      dict.FindBoolByDottedPath(prefs::kDefaultSearchProviderEnabled))
    return YES;
  return NO;
}

// Returns the text to be displayed by the managed Search Engine item.
- (NSString*)managedSearchEngineDetailText {
  const base::Value::Dict& dict = _browserState->GetPrefs()->GetDict(
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
      AuthenticationServiceFactory::GetForBrowserState(
          _browser->GetBrowserState());
  BOOL isSignedIn =
      authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
  return (isSignedIn && IsWebChannelsEnabled())
             ? l10n_util::GetNSString(IDS_IOS_DISCOVER_AND_FOLLOWING_FEED_TITLE)
             : l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE);
}

// Decides whether the default browser blue dot promo should be active, and adds
// the blue dot badge to the right settings row if it is.
- (void)maybeActivateDefaultBrowserBlueDotPromo:
    (TableViewDetailIconItem*)defaultBrowserCellItem {
  self.showingDefaultBrowserNotificationDot = NO;

  if (!_browserState) {
    return;
  }

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(_browserState);
  if (!tracker) {
    return;
  }

  if (ShouldTriggerDefaultBrowserBlueDotBadgeFeature(
          feature_engagement::kIPHiOSDefaultBrowserSettingsBadgeFeature,
          tracker)) {
    // Add the blue dot promo badge to the default browser row.
    defaultBrowserCellItem.showNotificationDot = YES;
    self.showingDefaultBrowserNotificationDot = YES;

    // If we've only started showing the blue dot recently (<6 hours), don't
    // notify the FET again that the promo is being shown, since we're not in a
    // new user session. We record the badge being shown per user session,
    // instead of per time it is shown since the badge needs to be shown accross
    // 3 user sessions.
    if (!HasRecentTimestampForKey(
            kMostRecentTimestampBlueDotPromoShownInSettingsMenu)) {
      tracker->NotifyEvent(
          feature_engagement::events::kBlueDotPromoSettingsShownNewSession);
    }
  }
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [self.applicationCommandsHandler showSignin:command baseViewController:self];
}

#pragma mark Sign in

- (void)showSignInWithIdentity:(id<SystemIdentity>)identity
                   promoAction:(signin_metrics::PromoAction)promoAction
                    completion:(ShowSigninCommandCompletionCallback)completion {
  DCHECK(!self.isSigninInProgress);
  self.isSigninInProgress = YES;
  __weak __typeof(self) weakSelf = self;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperationSigninAndSync
               identity:identity
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS
            promoAction:promoAction
               callback:^(BOOL success) {
                 if (completion)
                   completion(success);
                 [weakSelf didFinishSignin:success];
               }];
  [self.applicationCommandsHandler showSignin:command baseViewController:self];
}

- (void)didFinishSignin:(BOOL)signedIn {
  if (_settingsAreDismissed)
    return;

  // The sign-in is done. The sign-in promo cell or account cell can be
  // reloaded.
  DCHECK(self.isSigninInProgress);
  self.isSigninInProgress = NO;
  [self reloadData];
}

#pragma mark SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileSettingsClose"));
}

- (void)reportBackUserAction {
  // Not called for root settings controller.
  NOTREACHED();
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // Disconnect the sign-in mediator.
  DCHECK(!self.isSigninInProgress);
  [_signinPromoViewMediator disconnect];
  _signinPromoViewMediator = nil;

  // Stop children coordinators.
  [_googleServicesSettingsCoordinator stop];
  _googleServicesSettingsCoordinator.delegate = nil;
  _googleServicesSettingsCoordinator = nil;

  [_safetyCheckCoordinator stop];
  _safetyCheckCoordinator = nil;

  [_passwordsCoordinator stop];
  _passwordsCoordinator.delegate = nil;
  _passwordsCoordinator = nil;

  [_priceNotificationsCoordinator stop];
  _priceNotificationsCoordinator = nil;

  [_privacyCoordinator stop];
  _privacyCoordinator = nil;

  [_manageSyncSettingsCoordinator stop];
  _manageSyncSettingsCoordinator = nil;

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

  // Clear C++ ivars.
  _voiceLocaleCode.Destroy();
  _passwordCheckManager.reset();
  _browser = nullptr;
  _browserState = nullptr;

  _settingsAreDismissed = YES;
}

#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
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
            ios::TemplateURLServiceFactory::GetForBrowserState(_browserState)));
    [self reconfigureCellsForItems:@[ _defaultSearchEngineItem ]];
  }
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if ([_identity isEqual:identity]) {
    [self reloadAccountCell];
  }
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
    [self updateDiscoverSection];
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
  } else {
    NOTREACHED();
  }
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  [self setSafetyCheckIssueStateUnsafe:[self hasPasswordIssuesRemaining]];
}

- (void)insecureCredentialsDidChange {
  [self setSafetyCheckIssueStateUnsafe:[self hasPasswordIssuesRemaining]];
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
    BOOL passwordsEnabled =
        _browserState->GetPrefs()->GetBoolean(preferenceName);
    NSString* passwordsDetail =
        passwordsEnabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                         : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _passwordsDetailItem.detailText = passwordsDetail;
    [self reconfigureCellsForItems:@[ _passwordsDetailItem ]];
  }

  if (preferenceName == autofill::prefs::kAutofillProfileEnabled) {
    BOOL autofillProfileEnabled =
        autofill::prefs::IsAutofillProfileEnabled(_browserState->GetPrefs());
    NSString* detailText = autofillProfileEnabled
                               ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                               : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _autoFillProfileDetailItem.detailText = detailText;
    [self reconfigureCellsForItems:@[ _autoFillProfileDetailItem ]];
  }

  if (preferenceName == autofill::prefs::kAutofillCreditCardEnabled) {
    BOOL autofillCreditCardEnabled =
        autofill::prefs::IsAutofillCreditCardEnabled(_browserState->GetPrefs());
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

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  if (self.isSigninInProgress ||
      ![self.tableViewModel
          hasItemForItemType:SettingsItemTypeSigninPromo
           sectionIdentifier:SettingsSectionIdentifierSignIn]) {
    // Don't reload the sign-in promo if sign-in is in progress, to avoid having
    // UI glitches. The table view should be reloaded once the sign-in is
    // finished.
    return;
  }
  NSIndexPath* signinPromoCellIndexPath = [self.tableViewModel
      indexPathForItemType:SettingsItemTypeSigninPromo
         sectionIdentifier:SettingsSectionIdentifierSignIn];
  DCHECK(signinPromoCellIndexPath.item != NSNotFound);
  TableViewSigninPromoItem* signinPromoItem =
      base::mac::ObjCCast<TableViewSigninPromoItem>(
          [self.tableViewModel itemAtIndexPath:signinPromoCellIndexPath]);
  if (signinPromoItem) {
    signinPromoItem.configurator = configurator;
    signinPromoItem.delegate = _signinPromoViewMediator;
    [self reconfigureCellsForItems:@[ signinPromoItem ]];
  }
}

- (void)signinPromoViewMediator:(SigninPromoViewMediator*)mediator
    shouldOpenSigninWithIdentity:(id<SystemIdentity>)identity
                     promoAction:(signin_metrics::PromoAction)promoAction
                      completion:
                          (ShowSigninCommandCompletionCallback)completion {
  [self showSignInWithIdentity:identity
                   promoAction:promoAction
                    completion:completion];
}

- (void)signinPromoViewMediatorCloseButtonWasTapped:
    (SigninPromoViewMediator*)mediator {
  [self reloadData];
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

#pragma mark - SafetyCheckCoordinatorDelegate

- (void)passwordsCoordinatorDidRemove:(PasswordsCoordinator*)coordinator {
  DCHECK_EQ(_passwordsCoordinator, coordinator);
  [_passwordsCoordinator stop];
  _passwordsCoordinator.delegate = nil;
  _passwordsCoordinator = nil;
}

#pragma mark - PriceNotificationsDelegate

- (void)priceNotificationsCoordinatorDidRemove:
    (PriceNotificationsCoordinator*)coordinator {
  DCHECK_EQ(_priceNotificationsCoordinator, coordinator);
  [_priceNotificationsCoordinator stop];
  _priceNotificationsCoordinator = nil;
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
  // See: -[SettingsTableViewController didFinishSignin:].
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

@end
