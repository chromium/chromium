// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"

#include <memory>

#include "base/feature_list.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/util.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/commands/settings_main_page_commands.h"
#import "ios/chrome/browser/ui/settings/about_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/bandwidth_management_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/content_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/material_cell_catalog_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/search_engine_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/table_cell_catalog_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/voice_search_table_view_controller.h"
#import "ios/chrome/browser/ui/signin_interaction/public/signin_presenter.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_coordinator.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/browser/voice/speech_input_locale_config.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_prefs.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSettingsTableViewId = @"kSettingsTableViewId";
NSString* const kSettingsSignInCellId = @"kSettingsSignInCellId";
NSString* const kSettingsAccountCellId = @"kSettingsAccountCellId";
NSString* const kSettingsSearchEngineCellId = @"Search Engine";
NSString* const kSettingsVoiceSearchCellId = @"Voice Search Settings";

namespace {

const CGFloat kAccountProfilePhotoDimension = 40.0f;

NSString* const kSyncAndGoogleServicesImageName = @"sync_and_google_services";
NSString* const kSyncAndGoogleServicesSyncErrorImageName =
    @"sync_and_google_services_sync_error";
NSString* const kSyncAndGoogleServicesSyncOffImageName =
    @"sync_and_google_services_sync_off";
NSString* const kSyncAndGoogleServicesSyncOnImageName =
    @"sync_and_google_services_sync_on";
NSString* const kSettingsSearchEngineImageName = @"settings_search_engine";
NSString* const kSettingsPasswordsImageName = @"settings_passwords";
NSString* const kSettingsAutofillCreditCardImageName =
    @"settings_payment_methods";
NSString* const kSettingsAutofillProfileImageName = @"settings_addresses";
NSString* const kSettingsVoiceSearchImageName = @"settings_voice_search";
NSString* const kSettingsPrivacyImageName = @"settings_privacy";
NSString* const kSettingsLanguageSettingsImageName =
    @"settings_language_settings";
NSString* const kSettingsContentSettingsImageName =
    @"settings_content_settings";
NSString* const kSettingsBandwidthImageName = @"settings_bandwidth";
NSString* const kSettingsAboutChromeImageName = @"settings_about_chrome";
NSString* const kSettingsDebugImageName = @"settings_debug";
NSString* const kSettingsArticleSuggestionsImageName =
    @"settings_article_suggestions";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSignIn = kSectionIdentifierEnumZero,
  SectionIdentifierAccount,
  SectionIdentifierBasics,
  SectionIdentifierAdvanced,
  SectionIdentifierInfo,
  SectionIdentifierDebug,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSignInButton = kItemTypeEnumZero,
  ItemTypeSigninPromo,
  ItemTypeAccount,
  ItemGoogleServices,
  ItemTypeHeader,
  ItemTypeSearchEngine,
  ItemTypePasswords,
  ItemTypeAutofillCreditCard,
  ItemTypeAutofillProfile,
  ItemTypeVoiceSearch,
  ItemTypePrivacy,
  ItemTypeLanguageSettings,
  ItemTypeContentSettings,
  ItemTypeBandwidth,
  ItemTypeAboutChrome,
  ItemTypeMemoryDebugging,
  ItemTypeViewSource,
  ItemTypeCollectionCellCatalog,
  ItemTypeTableCellCatalog,
  ItemTypeArticlesForYou,
};

#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
NSString* kDevViewSourceKey = @"DevViewSource";
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

}  // namespace

#pragma mark - SettingsTableViewController

@interface SettingsTableViewController () <
    BooleanObserver,
    ChromeIdentityServiceObserver,
    GoogleServicesSettingsCoordinatorDelegate,
    IdentityManagerObserverBridgeDelegate,
    PrefObserverDelegate,
    SettingsControllerProtocol,
    SearchEngineObserving,
    SettingsMainPageCommands,
    SigninPresenter,
    SigninPromoViewConsumer,
    SyncObserverModelBridge> {
  // The browser where the settings are being displayed.
  Browser* _browser;
  // The browser state for |_browser|. Never off the record.
  ios::ChromeBrowserState* _browserState;  // weak
  // Bridge for TemplateURLServiceObserver.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserverBridge;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  // Whether the impression of the Signin button has already been recorded.
  BOOL _hasRecordedSigninImpression;
  // PrefBackedBoolean for ShowMemoryDebugTools switch.
  PrefBackedBoolean* _showMemoryDebugToolsEnabled;
  // PrefBackedBoolean for ArticlesForYou switch.
  PrefBackedBoolean* _articlesEnabled;
  // The item related to the switch for the show suggestions setting.
  SettingsSwitchItem* _showMemoryDebugToolsItem;
  // The item related to the switch for the show suggestions setting.
  SettingsSwitchItem* _articlesForYouItem;

  // Mediator to configure the sign-in promo cell. Also used to received
  // identity update notifications.
  SigninPromoViewMediator* _signinPromoViewMediator;
  GoogleServicesSettingsCoordinator* _googleServicesSettingsCoordinator;

  // Cached resized profile image.
  UIImage* _resizedImage;
  __weak UIImage* _oldImage;

  // Identity object and observer used for Account Item refresh.
  ChromeIdentity* _identity;
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;

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
  TableViewDetailIconItem* _passwordsDetailItem;
  TableViewDetailIconItem* _autoFillProfileDetailItem;
  TableViewDetailIconItem* _autoFillCreditCardDetailItem;

  // YES if the user used at least once the sign-in promo view buttons.
  BOOL _signinStarted;
  // YES if view has been dismissed.
  BOOL _settingsHasBeenDismissed;
}

@property(nonatomic, readonly, weak) id<ApplicationCommands, BrowserCommands>
    dispatcher;

// The SigninInteractionCoordinator that presents Sign In UI for the
// Settings page.
@property(nonatomic, strong)
    SigninInteractionCoordinator* signinInteractionCoordinator;

// Stops observing browser state services. This is required during the shutdown
// phase to avoid observing services for a profile that is being killed.
- (void)stopBrowserStateServiceObservers;

@end

@implementation SettingsTableViewController
@synthesize settingsMainPageDispatcher = _settingsMainPageDispatcher;
@synthesize dispatcher = _dispatcher;
@synthesize signinInteractionCoordinator = _signinInteractionCoordinator;

#pragma mark Initialization

- (instancetype)initWithBrowser:(Browser*)browser
                     dispatcher:
                         (id<ApplicationCommands, BrowserCommands>)dispatcher {
  DCHECK(browser);
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _browser = browser;
    _browserState = _browser->GetBrowserState();
    self.title = l10n_util::GetNSStringWithFixup(IDS_IOS_SETTINGS_TITLE);
    _searchEngineObserverBridge.reset(new SearchEngineObserverBridge(
        self,
        ios::TemplateURLServiceFactory::GetForBrowserState(_browserState)));
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(_browserState);
    // It is expected that |identityManager| should never be nil except in
    // tests. In that case, the tests should be fixed.
    DCHECK(identityManager);
    _identityObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(identityManager, self));
    syncer::SyncService* syncService =
        ProfileSyncServiceFactory::GetForBrowserState(_browserState);
    _syncObserverBridge.reset(new SyncObserverBridge(self, syncService));

    _showMemoryDebugToolsEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()
                   prefName:prefs::kShowMemoryDebuggingTools];
    [_showMemoryDebugToolsEnabled setObserver:self];

    AuthenticationService* authService =
        AuthenticationServiceFactory::GetForBrowserState(_browserState);
    _identity = authService->GetAuthenticatedIdentity();
    _identityServiceObserver.reset(
        new ChromeIdentityServiceObserverBridge(self));

    PrefService* prefService = _browserState->GetPrefs();

    _articlesEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::kArticlesForYouEnabled];
    [_articlesEnabled setObserver:self];

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

    _settingsMainPageDispatcher = self;
    _dispatcher = dispatcher;

    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

- (void)dealloc {
  DCHECK(_settingsHasBeenDismissed)
      << "-settingsWillBeDismissed must be called before -dealloc";
}

- (void)stopBrowserStateServiceObservers {
  _syncObserverBridge.reset();
  _identityObserverBridge.reset();
  _identityServiceObserver.reset();
  [_showMemoryDebugToolsEnabled setObserver:nil];
  [_articlesEnabled setObserver:nil];
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

  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(_browserState);
  if (!authService->IsAuthenticated()) {
    // Sign in section
    [model addSectionWithIdentifier:SectionIdentifierSignIn];
    if ([SigninPromoViewMediator
            shouldDisplaySigninPromoViewWithAccessPoint:
                signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS
                                           browserState:_browserState]) {
      if (!_signinPromoViewMediator) {
        _signinPromoViewMediator = [[SigninPromoViewMediator alloc]
            initWithBrowserState:_browserState
                     accessPoint:signin_metrics::AccessPoint::
                                     ACCESS_POINT_SETTINGS
                       presenter:self /* id<SigninPresenter> */];
        _signinPromoViewMediator.consumer = self;
      }
    } else {
      [_signinPromoViewMediator signinPromoViewIsRemoved];
      _signinPromoViewMediator = nil;
    }
    [model addItem:[self signInTextItem]
        toSectionWithIdentifier:SectionIdentifierSignIn];
  } else {
    // Account section
    [model addSectionWithIdentifier:SectionIdentifierAccount];
    _hasRecordedSigninImpression = NO;
    [_signinPromoViewMediator signinPromoViewIsRemoved];
    _signinPromoViewMediator = nil;
    [model addItem:[self accountCellItem]
        toSectionWithIdentifier:SectionIdentifierAccount];
  }
  if (![model hasSectionForSectionIdentifier:SectionIdentifierAccount]) {
    // Add the Account section for the Google services cell, if the user is
    // signed-out.
    [model addSectionWithIdentifier:SectionIdentifierAccount];
  }
  [model addItem:[self googleServicesCellItem]
      toSectionWithIdentifier:SectionIdentifierAccount];

  // Basics section
  [model addSectionWithIdentifier:SectionIdentifierBasics];
  [model addItem:[self searchEngineDetailItem]
      toSectionWithIdentifier:SectionIdentifierBasics];
  [model addItem:[self passwordsDetailItem]
      toSectionWithIdentifier:SectionIdentifierBasics];
  [model addItem:[self AutoFillCreditCardDetailItem]
      toSectionWithIdentifier:SectionIdentifierBasics];
  [model addItem:[self autoFillProfileDetailItem]
      toSectionWithIdentifier:SectionIdentifierBasics];

  // Advanced Section
  [model addSectionWithIdentifier:SectionIdentifierAdvanced];
  [model addItem:[self voiceSearchDetailItem]
      toSectionWithIdentifier:SectionIdentifierAdvanced];
  [model addItem:[self privacyDetailItem]
      toSectionWithIdentifier:SectionIdentifierAdvanced];
  _articlesForYouItem = [self articlesForYouSwitchItem];
  [model addItem:_articlesForYouItem
      toSectionWithIdentifier:SectionIdentifierAdvanced];
  if (base::FeatureList::IsEnabled(kLanguageSettings)) {
    [model addItem:[self languageSettingsDetailItem]
        toSectionWithIdentifier:SectionIdentifierAdvanced];
  }
  [model addItem:[self contentSettingsDetailItem]
      toSectionWithIdentifier:SectionIdentifierAdvanced];
  [model addItem:[self bandwidthManagementDetailItem]
      toSectionWithIdentifier:SectionIdentifierAdvanced];

  // Info Section
  [model addSectionWithIdentifier:SectionIdentifierInfo];
  [model addItem:[self aboutChromeDetailItem]
      toSectionWithIdentifier:SectionIdentifierInfo];

  // Debug Section
  if ([self hasDebugSection]) {
    [model addSectionWithIdentifier:SectionIdentifierDebug];
  }

  if (experimental_flags::IsMemoryDebuggingEnabled()) {
    _showMemoryDebugToolsItem = [self showMemoryDebugSwitchItem];
    [model addItem:_showMemoryDebugToolsItem
        toSectionWithIdentifier:SectionIdentifierDebug];
  }

#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
  [model addItem:[self viewSourceSwitchItem]
      toSectionWithIdentifier:SectionIdentifierDebug];
  [model addItem:[self collectionViewCatalogDetailItem]
      toSectionWithIdentifier:SectionIdentifierDebug];
  [model addItem:[self tableViewCatalogDetailItem]
      toSectionWithIdentifier:SectionIdentifierDebug];
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
}

#pragma mark - Model Items

- (TableViewItem*)signInTextItem {
  if (_signinPromoViewMediator) {
    TableViewSigninPromoItem* signinPromoItem =
        [[TableViewSigninPromoItem alloc] initWithType:ItemTypeSigninPromo];
    signinPromoItem.text =
        l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_SETTINGS_WITH_UNITY);
    signinPromoItem.configurator =
        [_signinPromoViewMediator createConfigurator];
    signinPromoItem.delegate = _signinPromoViewMediator;
    [_signinPromoViewMediator signinPromoViewIsVisible];
    return signinPromoItem;
  }
  if (!_hasRecordedSigninImpression) {
    // Once the Settings are open, this button impression will at most be
    // recorded once until they are closed.
    signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
        signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
    _hasRecordedSigninImpression = YES;
  }
  AccountSignInItem* signInTextItem =
      [[AccountSignInItem alloc] initWithType:ItemTypeSignInButton];
  signInTextItem.accessibilityIdentifier = kSettingsSignInCellId;
  signInTextItem.detailText =
      l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SUBTITLE);

  return signInTextItem;
}

- (TableViewItem*)googleServicesCellItem {
  // TODO(crbug.com/805214): This branded icon image needs to come from
  // BrandedImageProvider.
  TableViewImageItem* googleServicesItem =
      [[TableViewImageItem alloc] initWithType:ItemGoogleServices];
  googleServicesItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  googleServicesItem.title =
      l10n_util::GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE);
  [self updateGoogleServicesItem:googleServicesItem];
  return googleServicesItem;
}

- (TableViewItem*)accountCellItem {
  TableViewAccountItem* identityAccountItem =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
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

  _defaultSearchEngineItem =
      [self detailItemWithType:ItemTypeSearchEngine
                          text:l10n_util::GetNSString(
                                   IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)
                    detailText:defaultSearchEngineName
                 iconImageName:kSettingsSearchEngineImageName];
  _defaultSearchEngineItem.accessibilityIdentifier =
      kSettingsSearchEngineCellId;
  return _defaultSearchEngineItem;
}

- (TableViewItem*)passwordsDetailItem {
  BOOL passwordsEnabled = _browserState->GetPrefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService);
  NSString* passwordsDetail = passwordsEnabled
                                  ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _passwordsDetailItem =
      [self detailItemWithType:ItemTypePasswords
                          text:l10n_util::GetNSString(IDS_IOS_PASSWORDS)
                    detailText:passwordsDetail
                 iconImageName:kSettingsPasswordsImageName];

  return _passwordsDetailItem;
}

- (TableViewItem*)AutoFillCreditCardDetailItem {
  BOOL autofillCreditCardEnabled =
      autofill::prefs::IsCreditCardAutofillEnabled(_browserState->GetPrefs());
  NSString* detailText = autofillCreditCardEnabled
                             ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                             : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _autoFillCreditCardDetailItem = [self
      detailItemWithType:ItemTypeAutofillCreditCard
                    text:l10n_util::GetNSString(IDS_AUTOFILL_PAYMENT_METHODS)
              detailText:detailText
           iconImageName:kSettingsAutofillCreditCardImageName];

  return _autoFillCreditCardDetailItem;
}

- (TableViewItem*)autoFillProfileDetailItem {
  BOOL autofillProfileEnabled =
      autofill::prefs::IsProfileAutofillEnabled(_browserState->GetPrefs());
  NSString* detailText = autofillProfileEnabled
                             ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                             : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  _autoFillProfileDetailItem =
      [self detailItemWithType:ItemTypeAutofillProfile
                          text:l10n_util::GetNSString(
                                   IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE)
                    detailText:detailText
                 iconImageName:kSettingsAutofillProfileImageName];

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
      [self detailItemWithType:ItemTypeVoiceSearch
                          text:l10n_util::GetNSString(
                                   IDS_IOS_VOICE_SEARCH_SETTING_TITLE)
                    detailText:languageName
                 iconImageName:kSettingsVoiceSearchImageName];
  _voiceSearchDetailItem.accessibilityIdentifier = kSettingsVoiceSearchCellId;
  return _voiceSearchDetailItem;
}

- (TableViewItem*)privacyDetailItem {
  return
      [self detailItemWithType:ItemTypePrivacy
                          text:l10n_util::GetNSString(
                                   IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY)
                    detailText:nil
                 iconImageName:kSettingsPrivacyImageName];
}

- (TableViewItem*)languageSettingsDetailItem {
  return [self
      detailItemWithType:ItemTypeLanguageSettings
                    text:l10n_util::GetNSString(IDS_IOS_LANGUAGE_SETTINGS_TITLE)
              detailText:nil
           iconImageName:kSettingsLanguageSettingsImageName];
}

- (TableViewItem*)contentSettingsDetailItem {
  return [self
      detailItemWithType:ItemTypeContentSettings
                    text:l10n_util::GetNSString(IDS_IOS_CONTENT_SETTINGS_TITLE)
              detailText:nil
           iconImageName:kSettingsContentSettingsImageName];
}

- (TableViewItem*)bandwidthManagementDetailItem {
  return [self detailItemWithType:ItemTypeBandwidth
                             text:l10n_util::GetNSString(
                                      IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS)
                       detailText:nil
                    iconImageName:kSettingsBandwidthImageName];
}

- (TableViewItem*)aboutChromeDetailItem {
  return [self detailItemWithType:ItemTypeAboutChrome
                             text:l10n_util::GetNSString(IDS_IOS_PRODUCT_NAME)
                       detailText:nil
                    iconImageName:kSettingsAboutChromeImageName];
}

- (SettingsSwitchItem*)showMemoryDebugSwitchItem {
  SettingsSwitchItem* showMemoryDebugSwitchItem =
      [self switchItemWithType:ItemTypeMemoryDebugging
                         title:@"Show memory debug tools"
                 iconImageName:kSettingsDebugImageName
               withDefaultsKey:nil];
  showMemoryDebugSwitchItem.on = [_showMemoryDebugToolsEnabled value];

  return showMemoryDebugSwitchItem;
}

- (SettingsSwitchItem*)articlesForYouSwitchItem {
  SettingsSwitchItem* articlesForYouSwitchItem =
      [self switchItemWithType:ItemTypeArticlesForYou
                         title:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_SUGGESTIONS_SETTING_TITLE)
                 iconImageName:kSettingsArticleSuggestionsImageName
               withDefaultsKey:nil];
  articlesForYouSwitchItem.on = [_articlesEnabled value];

  return articlesForYouSwitchItem;
}
#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

- (SettingsSwitchItem*)viewSourceSwitchItem {
  return [self switchItemWithType:ItemTypeViewSource
                            title:@"View source menu"
                    iconImageName:kSettingsDebugImageName
                  withDefaultsKey:kDevViewSourceKey];
}

- (TableViewDetailIconItem*)collectionViewCatalogDetailItem {
  return [self detailItemWithType:ItemTypeCollectionCellCatalog
                             text:@"Collection Cell Catalog"
                       detailText:nil
                    iconImageName:kSettingsDebugImageName];
}

- (TableViewDetailIconItem*)tableViewCatalogDetailItem {
  return [self detailItemWithType:ItemTypeTableCellCatalog
                             text:@"TableView Cell Catalog"
                       detailText:nil
                    iconImageName:kSettingsDebugImageName];
}
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

#pragma mark Item Constructors

- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                          text:(NSString*)text
                                    detailText:(NSString*)detailText
                                 iconImageName:(NSString*)iconImageName {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.detailText = detailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.iconImageName = iconImageName;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;

  return detailItem;
}

- (SettingsSwitchItem*)switchItemWithType:(NSInteger)type
                                    title:(NSString*)title
                            iconImageName:(NSString*)iconImageName
                          withDefaultsKey:(NSString*)key {
  SettingsSwitchItem* switchItem =
      [[SettingsSwitchItem alloc] initWithType:type];
  switchItem.text = title;
  switchItem.iconImageName = iconImageName;
  if (key) {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    switchItem.on = [defaults boolForKey:key];
  }

  return switchItem;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if ([cell isKindOfClass:[TableViewDetailIconCell class]]) {
    TableViewDetailIconCell* detailCell =
        base::mac::ObjCCastStrict<TableViewDetailIconCell>(cell);
    if (itemType == ItemTypePasswords) {
      scoped_refptr<password_manager::PasswordStore> passwordStore =
          IOSChromePasswordStoreFactory::GetForBrowserState(
              _browserState, ServiceAccessType::EXPLICIT_ACCESS);
      if (!passwordStore) {
        // The password store factory returns a NULL password store if something
        // goes wrong during the password store initialization. Disable the save
        // passwords cell in this case.
        LOG(ERROR) << "Save passwords cell was disabled as the password store"
                      " cannot be created.";
        [detailCell setUserInteractionEnabled:NO];
        detailCell.textLabel.textColor = UIColor.cr_secondaryLabelColor;
        return cell;
      }
    }

    [detailCell setUserInteractionEnabled:YES];
    detailCell.textLabel.textColor = UIColor.cr_labelColor;
  }

  switch (itemType) {
    case ItemTypeMemoryDebugging: {
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(memorySwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeArticlesForYou: {
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(articlesForYouSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeViewSource: {
#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(viewSourceSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
#else
      NOTREACHED();
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  id object = [self.tableViewModel itemAtIndexPath:indexPath];
  if ([object respondsToSelector:@selector(isEnabled)] &&
      ![object performSelector:@selector(isEnabled)]) {
    // Don't perform any action if the cell isn't enabled.
    return;
  }

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  UIViewController<SettingsRootViewControlling>* controller;

  switch (itemType) {
    case ItemTypeSignInButton:
      signin_metrics::RecordSigninUserActionForAccessPoint(
          signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
          signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
      [self showSignInWithIdentity:nil
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_NO_SIGNIN_PROMO
                        completion:nil];
      break;
    case ItemTypeAccount:
      controller = [[AccountsTableViewController alloc] initWithBrowser:_browser
                                              closeSettingsOnAddAccount:NO];
      break;
    case ItemGoogleServices:
      [self showSyncGoogleService];
      break;
    case ItemTypeSearchEngine:
      controller = [[SearchEngineTableViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case ItemTypePasswords:
      controller = [[PasswordsTableViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case ItemTypeAutofillCreditCard:
      controller = [[AutofillCreditCardTableViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case ItemTypeAutofillProfile:
      controller = [[AutofillProfileTableViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case ItemTypeVoiceSearch:
      controller = [[VoiceSearchTableViewController alloc]
          initWithPrefs:_browserState->GetPrefs()];
      break;
    case ItemTypePrivacy:
      controller = [[PrivacyTableViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case ItemTypeLanguageSettings: {
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
    case ItemTypeContentSettings:
      controller = [[ContentSettingsTableViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case ItemTypeBandwidth:
      controller = [[BandwidthManagementTableViewController alloc]
          initWithBrowserState:_browserState];
      break;
    case ItemTypeAboutChrome:
      controller = [[AboutChromeTableViewController alloc] init];
      break;
    case ItemTypeMemoryDebugging:
    case ItemTypeViewSource:
      // Taps on these don't do anything. They have a switch as accessory view
      // and only the switch is tappable.
      break;
    case ItemTypeCollectionCellCatalog:
      [self.settingsMainPageDispatcher showMaterialCellCatalog];
      break;
    case ItemTypeTableCellCatalog:
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

#pragma mark Switch Actions

- (void)memorySwitchToggled:(UISwitch*)sender {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:ItemTypeMemoryDebugging
                              sectionIdentifier:SectionIdentifierDebug];

  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [_showMemoryDebugToolsEnabled setValue:newSwitchValue];
}

- (void)articlesForYouSwitchToggled:(UISwitch*)sender {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:ItemTypeArticlesForYou
                              sectionIdentifier:SectionIdentifierAdvanced];

  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [_articlesEnabled setValue:newSwitchValue];
}

#if BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)
- (void)viewSourceSwitchToggled:(UISwitch*)sender {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:ItemTypeViewSource
                              sectionIdentifier:SectionIdentifierDebug];

  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);

  BOOL newSwitchValue = sender.isOn;
  switchItem.on = newSwitchValue;
  [self setBooleanNSUserDefaultsValue:newSwitchValue forKey:kDevViewSourceKey];
}
#endif  // BUILDFLAG(CHROMIUM_BRANDING) && !defined(NDEBUG)

#pragma mark Private methods

- (void)showSyncGoogleService {
  DCHECK(!_googleServicesSettingsCoordinator);
  _googleServicesSettingsCoordinator =
      [[GoogleServicesSettingsCoordinator alloc]
          initWithBaseViewController:self.navigationController
                             browser:_browser
                                mode:GoogleServicesSettingsModeSettings];
  _googleServicesSettingsCoordinator.dispatcher = self.dispatcher;
  _googleServicesSettingsCoordinator.navigationController =
      self.navigationController;
  _googleServicesSettingsCoordinator.delegate = self;
  [_googleServicesSettingsCoordinator start];
}

// Sets the NSUserDefaults BOOL |value| for |key|.
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
  _identity = authService->GetAuthenticatedIdentity();
  if (!_identity) {
    // This could occur during the sign out process. Just ignore as the account
    // cell will be replaced by the "Sign in" button.
    return;
  }
  identityAccountItem.image = [self userAccountImage];
  identityAccountItem.text = [_identity userFullName];
  identityAccountItem.detailText = _identity.userEmail;
}

- (void)reloadAccountCell {
  if (![self.tableViewModel hasItemForItemType:ItemTypeAccount
                             sectionIdentifier:SectionIdentifierAccount]) {
    return;
  }
  NSIndexPath* accountCellIndexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeAccount
                              sectionIdentifier:SectionIdentifierAccount];
  TableViewAccountItem* identityAccountItem =
      base::mac::ObjCCast<TableViewAccountItem>(
          [self.tableViewModel itemAtIndexPath:accountCellIndexPath]);
  if (identityAccountItem) {
    [self updateIdentityAccountItem:identityAccountItem];
    [self reconfigureCellsForItems:@[ identityAccountItem ]];
  }
}

// Updates the Google services item to display the right icon and status message
// in the detail text of the cell.
- (void)updateGoogleServicesItem:(TableViewImageItem*)googleServicesItem {
  googleServicesItem.detailTextColor = nil;
  syncer::SyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(_browserState);
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(_browserState);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(_browserState);
  if (!authService->IsAuthenticated()) {
    // No sync status when the user is not signed-in.
    googleServicesItem.detailText = nil;
    googleServicesItem.image =
        [UIImage imageNamed:kSyncAndGoogleServicesImageName];
  } else if (syncService->GetDisableReasons() &
             syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) {
    googleServicesItem.detailText = l10n_util::GetNSString(
        IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_DISABLBED_BY_ADMINISTRATOR_STATUS);
    googleServicesItem.image =
        [UIImage imageNamed:kSyncAndGoogleServicesSyncOffImageName];
  } else if (!syncSetupService->IsFirstSetupComplete()) {
    googleServicesItem.detailText =
        l10n_util::GetNSString(IDS_IOS_SYNC_SETUP_IN_PROGRESS);
    googleServicesItem.image =
        [UIImage imageNamed:kSyncAndGoogleServicesSyncOnImageName];
  } else if (!IsTransientSyncError(syncSetupService->GetSyncServiceState())) {
    googleServicesItem.detailTextColor = [UIColor colorNamed:kRedColor];
    googleServicesItem.detailText =
        GetSyncErrorDescriptionForSyncSetupService(syncSetupService);
    googleServicesItem.image =
        [UIImage imageNamed:kSyncAndGoogleServicesSyncErrorImageName];
  } else if (syncSetupService->IsSyncEnabled()) {
    googleServicesItem.detailText =
        l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SYNC_ON);
    googleServicesItem.image =
        [UIImage imageNamed:kSyncAndGoogleServicesSyncOnImageName];
  } else {
    googleServicesItem.detailText =
        l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SYNC_OFF);
    googleServicesItem.image =
        [UIImage imageNamed:kSyncAndGoogleServicesSyncOffImageName];
  }
  DCHECK(googleServicesItem.image);
}

// Updates and reloads the Google service cell.
- (void)reloadGoogleServicesCell {
  NSIndexPath* googleServicesCellIndexPath =
      [self.tableViewModel indexPathForItemType:ItemGoogleServices
                              sectionIdentifier:SectionIdentifierAccount];
  TableViewImageItem* googleServicesItem =
      base::mac::ObjCCast<TableViewImageItem>(
          [self.tableViewModel itemAtIndexPath:googleServicesCellIndexPath]);
  DCHECK(googleServicesItem);
  [self updateGoogleServicesItem:googleServicesItem];
  [self reconfigureCellsForItems:@[ googleServicesItem ]];
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [self.dispatcher showSignin:command baseViewController:self];
}

#pragma mark Sign in

- (void)showSignInWithIdentity:(ChromeIdentity*)identity
                   promoAction:(signin_metrics::PromoAction)promoAction
                    completion:(ShowSigninCommandCompletionCallback)completion {
  DCHECK(![self.signinInteractionCoordinator isActive]);
  if (!self.signinInteractionCoordinator) {
    self.signinInteractionCoordinator =
        [[SigninInteractionCoordinator alloc] initWithBrowser:_browser
                                                   dispatcher:self.dispatcher];
  }

  __weak SettingsTableViewController* weakSelf = self;
  [self.signinInteractionCoordinator
            signInWithIdentity:identity
                   accessPoint:signin_metrics::AccessPoint::
                                   ACCESS_POINT_SETTINGS
                   promoAction:promoAction
      presentingViewController:self.navigationController
                    completion:^(BOOL success) {
                      if (completion)
                        completion(success);
                      [weakSelf didFinishSignin:success];
                    }];
}

- (void)didFinishSignin:(BOOL)signedIn {
  // The sign-in is done. The sign-in promo cell or account cell can be
  // reloaded.
  if (!_settingsHasBeenDismissed)
    [self reloadData];
}

#pragma mark Material Cell Catalog

- (void)showMaterialCellCatalog {
  [self.navigationController
      pushViewController:[[MaterialCellCatalogViewController alloc] init]
                animated:YES];
}

#pragma mark SettingsControllerProtocol

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsHasBeenDismissed);
  [_googleServicesSettingsCoordinator stop];
  _googleServicesSettingsCoordinator.delegate = nil;
  _googleServicesSettingsCoordinator = nil;
  _settingsHasBeenDismissed = YES;
  [self.signinInteractionCoordinator cancel];
  [_signinPromoViewMediator signinPromoViewIsRemoved];
  _signinPromoViewMediator = nil;
  [self stopBrowserStateServiceObservers];
}

#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self reloadGoogleServicesCell];
}

#pragma mark - IdentityRefreshLogic

// Image used for loggedin user account that supports caching.
- (UIImage*)userAccountImage {
  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetChromeIdentityService()
                       ->GetCachedAvatarForIdentity(_identity);
  if (!image) {
    image = ios::GetChromeBrowserProvider()
                ->GetSigninResourcesProvider()
                ->GetDefaultAvatar();
    // No cached image, trigger a fetch, which will notify all observers
    // (including the corresponding AccountViewBase).
    ios::GetChromeBrowserProvider()
        ->GetChromeIdentityService()
        ->GetAvatarForIdentity(_identity, nil);
  }

  // If the currently used image has already been resized, use it.
  if (_resizedImage && _oldImage == image)
    return _resizedImage;

  _oldImage = image;

  // Resize the profile image.
  CGFloat dimension = kAccountProfilePhotoDimension;
  if (image.size.width != dimension || image.size.height != dimension) {
    image = ResizeImage(image, CGSizeMake(dimension, dimension),
                        ProjectionMode::kAspectFit);
  }
  _resizedImage = image;
  return _resizedImage;
}

#pragma mark - SearchEngineObserverBridge

- (void)searchEngineChanged {
  _defaultSearchEngineItem.detailText =
      base::SysUTF16ToNSString(GetDefaultSearchEngineName(
          ios::TemplateURLServiceFactory::GetForBrowserState(_browserState)));
  [self reconfigureCellsForItems:@[ _defaultSearchEngineItem ]];
}

#pragma mark ChromeIdentityServiceObserver

- (void)profileUpdate:(ChromeIdentity*)identity {
  if (identity == _identity) {
    [self reloadAccountCell];
  }
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _showMemoryDebugToolsEnabled) {
    // Update the Item.
    _showMemoryDebugToolsItem.on = [_showMemoryDebugToolsEnabled value];
    // Update the Cell.
    [self reconfigureCellsForItems:@[ _showMemoryDebugToolsItem ]];
  } else if (observableBoolean == _articlesEnabled) {
    _articlesForYouItem.on = [_articlesEnabled value];
    [self reconfigureCellsForItems:@[ _articlesForYouItem ]];
  } else {
    NOTREACHED();
  }
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
        autofill::prefs::IsProfileAutofillEnabled(_browserState->GetPrefs());
    NSString* detailText = autofillProfileEnabled
                               ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                               : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _autoFillProfileDetailItem.detailText = detailText;
    [self reconfigureCellsForItems:@[ _autoFillProfileDetailItem ]];
  }

  if (preferenceName == autofill::prefs::kAutofillCreditCardEnabled) {
    BOOL autofillCreditCardEnabled =
        autofill::prefs::IsCreditCardAutofillEnabled(_browserState->GetPrefs());
    NSString* detailText = autofillCreditCardEnabled
                               ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                               : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _autoFillCreditCardDetailItem.detailText = detailText;
    [self reconfigureCellsForItems:@[ _autoFillCreditCardDetailItem ]];
  }
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  DCHECK(![self.signinInteractionCoordinator isActive]);
  if (![self.tableViewModel hasItemForItemType:ItemTypeSigninPromo
                             sectionIdentifier:SectionIdentifierSignIn]) {
    return;
  }
  NSIndexPath* signinPromoCellIndexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeSigninPromo
                              sectionIdentifier:SectionIdentifierSignIn];
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
    shouldOpenSigninWithIdentity:(ChromeIdentity*)identity
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

#pragma mark - IdentityManagerObserverBridgeDelegate

// Notifies this controller that the sign in state has changed.
- (void)signinStateDidChange {
  // While the sign-in interaction coordinator is presenting UI, the TableView
  // should not be updated. Otherwise, it would lead to an UI glitch either
  // while the sign in UI is appearing or disappearing. The TableView will be
  // reloaded once the animation is finished.
  // See: -[SettingsTableViewController didFinishSignin:].
  if ([self.signinInteractionCoordinator isActive])
    return;
  // Sign in state changes are rare. Just reload the entire table when
  // this happens.
  [self reloadData];
}
- (void)onPrimaryAccountSet:(const CoreAccountInfo&)primaryAccountInfo {
  [self signinStateDidChange];
}

- (void)onPrimaryAccountCleared:
    (const CoreAccountInfo&)previousPrimaryAccountInfo {
  [self signinStateDidChange];
}

@end
