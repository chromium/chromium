// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/i18n/message_formatter.h"
#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/breadcrumbs/core/breadcrumbs_status.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/prefs/pref_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#import "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#import "components/supervised_user/core/browser/proto_fetcher_status.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "components/url_formatter/url_formatter.h"
#import "components/version_info/version_info.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/app_store_rating/ui_bundled/app_store_rating_scene_agent.h"
#import "ios/chrome/browser/app_store_rating/ui_bundled/features.h"
#import "ios/chrome/browser/appearance/ui_bundled/appearance_customization.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_browser_agent.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_loop_detection_util.h"
#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_scene_agent.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/geolocation/model/geolocation_manager.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator_delegate.h"
#import "ios/chrome/browser/incognito_interstitial/ui_bundled/incognito_interstitial_coordinator.h"
#import "ios/chrome/browser/incognito_interstitial/ui_bundled/incognito_interstitial_coordinator_delegate.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/intents/user_activity_browser_agent.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_policy_scene_agent.h"
#import "ios/chrome/browser/policy/ui_bundled/signin_policy_scene_agent.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy_scene_agent.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy_util.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/screenshot/model/screenshot_delegate.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_saving_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_scene_agent.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/ui/authentication/signin/account_switch/account_switch_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_notification_infobar_delegate.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/main/default_browser_promo_scene_agent.h"
#import "ios/chrome/browser/ui/main/incognito_blocker_scene_agent.h"
#import "ios/chrome/browser/ui/main/ui_blocker_scene_agent.h"
#import "ios/chrome/browser/ui/main/wrangled_browser.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_scene_agent.h"
#import "ios/chrome/browser/ui/promos_manager/utils.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/whats_new/promo/whats_new_scene_agent.h"
#import "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/page_placeholder_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/session_metrics.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_data.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/navigation_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Killswitch, can be removed around February 2024. If enabled,
// createInitialUI will call makeKeyAndVisible before mainCoordinator start.
// When disabled, this fix resolves a flicker when starting the app in light
// mode
BASE_FEATURE(kMakeKeyAndVisibleBeforeMainCoordinatorStart,
             "MakeKeyAndVisibleBeforeMainCoordinatorStart",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to control whether Search Intents (Widgets, Application
// Shortcuts menu) forcibly open a new tab, rather than reusing an
// existing NTP. See http://crbug.com/1363375 for details.
BASE_FEATURE(kForceNewTabForIntentSearch,
             "ForceNewTabForIntentSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A rough estimate of the expected duration of a view controller transition
// animation. It's used to temporarily disable mutally exclusive chrome
// commands that trigger a view controller presentation.
const int64_t kExpectedTransitionDurationInNanoSeconds = 0.2 * NSEC_PER_SEC;

// Used to update the current BVC mode if a new tab is added while the tab
// switcher view is being dismissed.  This is different than ApplicationMode in
// that it can be set to `NONE` when not in use.
enum class TabSwitcherDismissalMode { NONE, NORMAL, INCOGNITO };

// Key of the UMA IOS.MultiWindow.OpenInNewWindow histogram.
const char kMultiWindowOpenInNewWindowHistogram[] =
    "IOS.MultiWindow.OpenInNewWindow";

// TODO(crbug.com/40788009): Use the Authentication Service sign-in status API
// instead of this when available.
bool IsSigninForcedByPolicy() {
  BrowserSigninMode policy_mode = static_cast<BrowserSigninMode>(
      GetApplicationContext()->GetLocalState()->GetInteger(
          prefs::kBrowserSigninPolicy));
  return policy_mode == BrowserSigninMode::kForced;
}

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)

NSString* const kAddLotsOfTabs = @"AddLotsOfTabs";

void InjectUnrealizedWebStates(Browser* browser, int count) {
  WebStateList* web_state_list = browser->GetWebStateList();

  SessionRestorationService* service =
      SessionRestorationServiceFactory::GetForProfile(browser->GetProfile());

  auto scoped_lock = web_state_list->StartBatchOperation();
  for (int i = 0; i < count; ++i) {
    std::string string_url = base::StringPrintf("http://google.com/%d", i);

    // Create the serialized representation of a WebState
    // with one navigation to `string_url` (defaulting the
    // title to the URL).
    web::proto::WebStateStorage storage = web::CreateWebStateStorage(
        web::NavigationManager::WebLoadParams(GURL(string_url)),
        base::UTF8ToUTF16(string_url.c_str()),
        /*created_with_opener=*/false, web::UserAgentType::MOBILE,
        base::Time::Now());

    // Ask the SessionService to create an unrealized WebState
    // and to prepare itself for it to be added to `browser`.
    std::unique_ptr<web::WebState> web_state =
        service->CreateUnrealizedWebState(browser, std::move(storage));

    // Insert the new unrealized WebState in `browser`.
    // Need to activate one WebState otherwise the session
    // will not be saved with the legacy session storage.
    int index = browser->GetWebStateList()->count();
    web_state_list->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(
            index == 0 && !web_state_list->GetActiveWebState()));
  }
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

void InjectNTP(Browser* browser) {
  // Don't inject an NTP for an empty web state list.
  if (!browser->GetWebStateList()->count()) {
    return;
  }

  // Don't inject an NTP on an NTP.
  web::WebState* webState = browser->GetWebStateList()->GetActiveWebState();
  if (IsUrlNtp(webState->GetVisibleURL())) {
    return;
  }

  // Queue up start surface with active tab.
  StartSurfaceRecentTabBrowserAgent* browser_agent =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(browser);
  // This may be nil for an incognito browser.
  if (browser_agent) {
    browser_agent->SaveMostRecentTab();
  }

  // Inject a live NTP.
  web::WebState::CreateParams create_params(browser->GetProfile());
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(create_params);
  std::vector<std::unique_ptr<web::NavigationItem>> items;
  std::unique_ptr<web::NavigationItem> item(web::NavigationItem::Create());
  item->SetURL(GURL(kChromeUINewTabURL));
  items.push_back(std::move(item));
  web_state->GetNavigationManager()->Restore(0, std::move(items));
  if (!browser->GetProfile()->IsOffTheRecord()) {
    NewTabPageTabHelper::CreateForWebState(web_state.get());
    NewTabPageTabHelper::FromWebState(web_state.get())
        ->SetShowStartSurface(true);
  }
  browser->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());
}

// Updates `data` with the Family Link member role associated to the primary
// signed-in account, no-op if the account is not enrolled in Family Link.
void OnListFamilyMembersResponse(
    const std::string& primary_account_gaia,
    UserFeedbackData* data,
    const supervised_user::ProtoFetcherStatus& status,
    std::unique_ptr<kidsmanagement::ListMembersResponse> response) {
  if (!status.IsOk()) {
    return;
  }
  for (const kidsmanagement::FamilyMember& member : response->members()) {
    if (member.user_id() == primary_account_gaia) {
      data.familyMemberRole = base::SysUTF8ToNSString(
          supervised_user::FamilyRoleToString(member.role()));
      break;
    }
  }
}

}  // namespace

@interface SceneController () <ProfileStateObserver,
                               HistoryCoordinatorDelegate,
                               IncognitoInterstitialCoordinatorDelegate,
                               PasswordCheckupCoordinatorDelegate,
                               PolicyWatcherBrowserAgentObserving,
                               SettingsNavigationControllerDelegate,
                               SceneUIProvider,
                               SceneURLLoadingServiceDelegate,
                               TabGridCoordinatorDelegate,
                               WebStateListObserving> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListForwardingObserver;
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _incognitoWebStateObserver;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _mainWebStateObserver;
  std::unique_ptr<
      base::ScopedObservation<PolicyWatcherBrowserAgent,
                              PolicyWatcherBrowserAgentObserverBridge>>
      _policyWatcherObserver;

  // The scene level component for url loading. Is passed down to
  // profile level UrlLoadingService instances.
  std::unique_ptr<SceneUrlLoadingService> _sceneURLLoadingService;

  // Map recording the number of tabs in WebStateList before the batch
  // operation started.
  std::map<WebStateList*, int> _tabCountBeforeBatchOperation;

  // Fetches the Family Link member role asynchronously from KidsManagement API.
  std::unique_ptr<supervised_user::ListFamilyMembersFetcher>
      _family_members_fetcher;
}

// Navigation View controller for the settings.
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

// Coordinator for display the Password Checkup.
@property(nonatomic, strong)
    PasswordCheckupCoordinator* passwordCheckupCoordinator;

// Coordinator for displaying history.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;

// Coordinates the creation of PDF screenshots with the window's content.
@property(nonatomic, strong) ScreenshotDelegate* screenshotDelegate;

// The tab switcher command and the voice search commands can be sent by views
// that reside in a different UIWindow leading to the fact that the exclusive
// touch property will be ineffective and a command for processing both
// commands may be sent in the same run of the runloop leading to
// inconsistencies. Those two boolean indicate if one of those commands have
// been processed in the last 200ms in order to only allow processing one at
// a time.
// TODO(crbug.com/40445992):  Provide a general solution for handling mutually
// exclusive chrome commands sent at nearly the same time.
@property(nonatomic, assign) BOOL isProcessingTabSwitcherCommand;
@property(nonatomic, assign) BOOL isProcessingVoiceSearchCommand;

// If not NONE, the current BVC should be switched to this BVC on completion
// of tab switcher dismissal.
@property(nonatomic, assign)
    TabSwitcherDismissalMode modeToDisplayOnTabSwitcherDismissal;

// A property to track which action to perform after dismissing the tab
// switcher. This is used to ensure certain post open actions that are
// presented by the BVC to only be triggered when the BVC is active.
@property(nonatomic, readwrite)
    TabOpeningPostOpeningAction NTPActionAfterTabSwitcherDismissal;

// The main coordinator, lazily created the first time it is accessed. Manages
// the main view controller. This property should not be accessed before the
// browser has started up to the FOREGROUND stage.
@property(nonatomic, strong) TabGridCoordinator* mainCoordinator;

// YES while activating a new browser (often leading to dismissing the tab
// switcher.
@property(nonatomic, assign) BOOL activatingBrowser;

// YES if the scene has been backgrounded since it has last been
// SceneActivationLevelForegroundActive.
@property(nonatomic, assign) BOOL backgroundedSinceLastActivated;

// Wrangler to handle BVC and tab model creation, access, and related logic.
// Implements features exposed from this object through the
// BrowserViewInformation protocol.
@property(nonatomic, strong) BrowserViewWrangler* browserViewWrangler;
// The coordinator used to control sign-in UI flows. Lazily created the first
// time it is accessed. Use -[startSigninCoordinatorWithCompletion:] to start
// the coordinator.
@property(nonatomic, strong) SigninCoordinator* signinCoordinator;

// YES if the process of dismissing the sign-in prompt is from an external
// trigger and is currently ongoing. An external trigger isn't done from the
// signin prompt itself (i.e., tapping a button in the sign-in prompt that
// dismisses the prompt). For example, the -dismissModalDialogswithCompletion
// command is considered as an external trigger because it comes from something
// outside the sign-in prompt UI.
@property(nonatomic, assign) BOOL dismissingSigninPromptFromExternalTrigger;

// The coordinator used to present the Incognito interstitial on Incognito
// third-party intents. Created in
// `showIncognitoInterstitialWithUrlLoadParams:dismissOmnibox:completion:`
// and destroyed in
// `closePresentedViews`.
@property(nonatomic, strong)
    IncognitoInterstitialCoordinator* incognitoInterstitialCoordinator;

// YES if the Settings view is being dismissed.
@property(nonatomic, assign) BOOL dismissingSettings;

// The state of the scene controlled by this object.
@property(nonatomic, weak, readonly) SceneState* sceneState;

@end

@implementation SceneController

@synthesize startupParameters = _startupParameters;
@synthesize startupParametersAreBeingHandled =
    _startupParametersAreBeingHandled;

- (instancetype)initWithSceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    _sceneState = sceneState;
    [_sceneState addObserver:self];

    _sceneURLLoadingService = std::make_unique<SceneUrlLoadingService>();
    _sceneURLLoadingService->SetDelegate(self);

    _webStateListForwardingObserver =
        std::make_unique<WebStateListObserverBridge>(self);

    _policyWatcherObserverBridge =
        std::make_unique<PolicyWatcherBrowserAgentObserverBridge>(self);

  }
  return self;
}

- (void)setProfileState:(ProfileState*)profileState {
  DCHECK(!_sceneState.profileState);

  _sceneState.profileState = profileState;
  [profileState sceneStateConnected:_sceneState];
  [profileState addObserver:self];

  // Add agents.
  [_sceneState addAgent:[[UIBlockerSceneAgent alloc] init]];
  [_sceneState addAgent:[[IncognitoBlockerSceneAgent alloc] init]];
  [_sceneState
      addAgent:[[IncognitoReauthSceneAgent alloc]
                   initWithReauthModule:[[ReauthenticationModule alloc] init]]];
  [_sceneState addAgent:[[StartSurfaceSceneAgent alloc] init]];
  [_sceneState addAgent:[[SessionSavingSceneAgent alloc] init]];
  [_sceneState addAgent:[[LayoutGuideSceneAgent alloc] init]];
}

#pragma mark - Setters and getters

- (TabGridCoordinator*)mainCoordinator {
  if (!_mainCoordinator) {
    // Lazily create the main coordinator.
    TabGridCoordinator* tabGridCoordinator = [[TabGridCoordinator alloc]
                     initWithWindow:self.sceneState.window
         applicationCommandEndpoint:self
                     regularBrowser:self.mainInterface.browser
                    inactiveBrowser:self.mainInterface.inactiveBrowser
                   incognitoBrowser:self.incognitoInterface.browser];
    tabGridCoordinator.delegate = self;
    _mainCoordinator = tabGridCoordinator;
  }
  return _mainCoordinator;
}

- (WrangledBrowser*)mainInterface {
  return self.browserViewWrangler.mainInterface;
}

- (WrangledBrowser*)currentInterface {
  return self.browserViewWrangler.currentInterface;
}

- (WrangledBrowser*)incognitoInterface {
  return self.browserViewWrangler.incognitoInterface;
}

- (id<BrowserProviderInterface>)browserProviderInterface {
  return self.browserViewWrangler;
}

- (void)setStartupParameters:(AppStartupParameters*)parameters {
  _startupParameters = parameters;
  self.startupParametersAreBeingHandled = NO;

  if (parameters.openedViaFirstPartyScheme) {
    [[NonModalDefaultBrowserPromoSchedulerSceneAgent
        agentFromScene:self.sceneState] logUserEnteredAppViaFirstPartyScheme];
  }

  Browser* mainBrowser =
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser;
  if (!mainBrowser) {
    return;
  }

  ProfileIOS* profile = mainBrowser->GetProfile();
  if (!profile) {
    return;
  }

  if (parameters.openedViaWidgetScheme) {
    // Notify Default Browser promo that user opened Chrome with widget.
    default_browser::NotifyStartWithWidget(
        feature_engagement::TrackerFactory::GetForProfile(profile));
  }
  if (parameters.openedWithURL) {
    // An HTTP(S) URL open that opened Chrome (e.g. default browser open or
    // explictly opened from first party apps) should be logged as significant
    // activity for a potential user that would want Chrome as their default
    // browser in case the user changes away from Chrome. This will leave a
    // trace of this activity for re-prompting.
    default_browser::NotifyStartWithURL(
        feature_engagement::TrackerFactory::GetForProfile(profile));
  }
}

- (BOOL)isTabGridVisible {
  return self.mainCoordinator.isTabGridActive;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  ProfileState* profileState = self.sceneState.profileState;
  [self transitionToSceneActivationLevel:level
                        profileInitStage:profileState.initStage];
}

- (void)handleExternalIntents {
  if (![self canHandleIntents]) {
    return;
  }
  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(self.currentInterface.browser);
  // Handle URL opening from
  // `UIWindowSceneDelegate scene:willConnectToSession:options:`.
  for (UIOpenURLContext* context in self.sceneState.connectionOptions
           .URLContexts) {
    URLOpenerParams* params =
        [[URLOpenerParams alloc] initWithUIOpenURLContext:context];
    [self openTabFromLaunchWithParams:params
                   startupInformation:self.sceneState.appState
                                          .startupInformation];
  }
  if (self.sceneState.connectionOptions.shortcutItem) {
    userActivityBrowserAgent->Handle3DTouchApplicationShortcuts(
        self.sceneState.connectionOptions.shortcutItem);
  }

  // See if this scene launched as part of a multiwindow URL opening.
  // If so, load that URL (this also creates a new tab to load the URL
  // in). No other UI will show in this case.
  NSUserActivity* activityWithCompletion;
  for (NSUserActivity* activity in self.sceneState.connectionOptions
           .userActivities) {
    if (ActivityIsURLLoad(activity)) {
      UrlLoadParams params = LoadParamsFromActivity(activity);
      ApplicationMode mode = params.in_incognito ? ApplicationMode::INCOGNITO
                                                 : ApplicationMode::NORMAL;
      [self openOrReuseTabInMode:mode
               withUrlLoadParams:params
             tabOpenedCompletion:nil];
    } else if (ActivityIsTabMove(activity)) {
      if ([self isTabActivityValid:activity]) {
        [self handleTabMoveActivity:activity];
      } else {
        // If the tab does not exist, open a new tab.
        UrlLoadParams params =
            UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
        ApplicationMode mode = self.currentInterface.incognito
                                   ? ApplicationMode::INCOGNITO
                                   : ApplicationMode::NORMAL;
        [self openOrReuseTabInMode:mode
                 withUrlLoadParams:params
               tabOpenedCompletion:nil];
      }
    } else if (!activityWithCompletion) {
      // Completion involves user interaction.
      // Only one can be triggered.
      activityWithCompletion = activity;
    }
  }
  if (activityWithCompletion) {
    // This function is called when the scene is activated (or unblocked).
    // Consider the scene as still not active at this point as the handling
    // of startup parameters is not yet done (and will be later in this
    // function).
    userActivityBrowserAgent->ContinueUserActivity(activityWithCompletion, NO);
  }
  self.sceneState.connectionOptions = nil;

  if (self.startupParameters) {
    if ([self isIncognitoForced]) {
      [self.startupParameters
          setUnexpectedMode:self.startupParameters.applicationMode ==
                            ApplicationModeForTabOpening::NORMAL];
      // When only incognito mode is available.
      [self.startupParameters
          setApplicationMode:ApplicationModeForTabOpening::INCOGNITO];
    } else if ([self isIncognitoDisabled]) {
      [self.startupParameters
          setUnexpectedMode:self.startupParameters.applicationMode ==
                            ApplicationModeForTabOpening::INCOGNITO];
      // When incognito mode is disabled.
      [self.startupParameters
          setApplicationMode:ApplicationModeForTabOpening::NORMAL];
    }

    userActivityBrowserAgent->RouteToCorrectTab();

    // Show a toast if the browser is opened in an unexpected mode.
    if (self.startupParameters.isUnexpectedMode) {
      [self showToastWhenOpenExternalIntentInUnexpectedMode];
    }
  }
}

// Handles a tab move activity as part of an intent when launching a
// scene. This should only ever be an intent generated by Chrome.
- (void)handleTabMoveActivity:(NSUserActivity*)activity {
  DCHECK(ActivityIsTabMove(activity));
  BOOL incognito = GetIncognitoFromTabMoveActivity(activity);
  web::WebStateID tabID = GetTabIDFromActivity(activity);

  WrangledBrowser* interface = self.currentInterface;

  // It's expected that the current interface matches `incognito`.
  DCHECK(interface.incognito == incognito);

  // Move the tab to the current interface's browser.
  MoveTabToBrowser(tabID, interface.browser, /*destination_tab_index=*/0);
}

- (void)recordWindowCreationForSceneState:(SceneState*)sceneState {
  // Don't record window creation for single-window environments
  if (!base::ios::IsMultipleScenesSupported()) {
    return;
  }

  // Don't record restored window creation.
  if (sceneState.currentOrigin == WindowActivityRestoredOrigin) {
    return;
  }

  // If there's only one connected scene, and it isn't being restored, this
  // must be the initial app launch with scenes, so don't record the window
  // creation.
  if (sceneState.profileState.connectedScenes.count <= 1) {
    return;
  }

  base::UmaHistogramEnumeration(kMultiWindowOpenInNewWindowHistogram,
                                sceneState.currentOrigin);
}

- (void)sceneState:(SceneState*)sceneState
    hasPendingURLs:(NSSet<UIOpenURLContext*>*)URLContexts {
  DCHECK(URLContexts);
  // It is necessary to reset the URLContextsToOpen after opening them.
  // Handle the opening asynchronously to avoid interfering with potential
  // other observers.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self openURLContexts:sceneState.URLContextsToOpen];
    self.sceneState.URLContextsToOpen = nil;
  });
}

- (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:
                       (void (^)(BOOL succeeded))completionHandler {
  if (self.sceneState.profileState.initStage <= ProfileInitStage::kUIReady ||
      !self.currentInterface.profile) {
    // Don't handle the intent if the browser UI objects aren't yet initialized.
    // This is the case when the app is in safe mode or may be the case when the
    // app is going through an odd sequence of lifecyle events (shouldn't happen
    // but happens somehow), see crbug.com/1211006 for more details.
    return;
  }

  self.sceneState.startupHadExternalIntent = YES;

  // Perform the action in incognito when only incognito mode is available.
  if ([self isIncognitoForced]) {
    [self.startupParameters
        setApplicationMode:ApplicationModeForTabOpening::INCOGNITO];
  }
  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(self.currentInterface.browser);
  BOOL handledShortcutItem =
      userActivityBrowserAgent->Handle3DTouchApplicationShortcuts(shortcutItem);
  if (completionHandler) {
    completionHandler(handledShortcutItem);
  }
}

- (void)sceneState:(SceneState*)sceneState
    receivedUserActivity:(NSUserActivity*)userActivity {
  if (!userActivity) {
    return;
  }

  if (self.sceneState.profileState.initStage <= ProfileInitStage::kUIReady ||
      !self.currentInterface.profile) {
    // Don't handle the intent if the browser UI objects aren't yet initialized.
    // This is the case when the app is in safe mode or may be the case when the
    // app is going through an odd sequence of lifecyle events (shouldn't happen
    // but happens somehow), see crbug.com/1211006 for more details.
    return;
  }

  BOOL sceneIsActive = [self canHandleIntents];
  self.sceneState.startupHadExternalIntent = YES;

  PrefService* prefs = self.currentInterface.profile->GetPrefs();
  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(self.currentInterface.browser);
  if (IsIncognitoPolicyApplied(prefs) &&
      !userActivityBrowserAgent->ProceedWithUserActivity(userActivity)) {
    // If users request opening url in a unavailable mode, don't open the url
    // but show a toast.
    [self showToastWhenOpenExternalIntentInUnexpectedMode];
  } else {
    userActivityBrowserAgent->ContinueUserActivity(userActivity, sceneIsActive);
  }

  if (sceneIsActive) {
    // It is necessary to reset the pendingUserActivity after handling it.
    // Handle the reset asynchronously to avoid interfering with other
    // observers.
    dispatch_async(dispatch_get_main_queue(), ^{
      self.sceneState.pendingUserActivity = nil;
    });
  }
}

- (void)sceneStateDidHideModalOverlay:(SceneState*)sceneState {
  [self handleExternalIntents];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  [self transitionToSceneActivationLevel:self.sceneState.activationLevel
                        profileInitStage:nextInitStage];
}

#pragma mark - private

// Creates, if needed, and presents saved passwords settings. Assumes all modal
// dialods are dismissed and `baseViewController` is available to present.
- (void)showSavedPasswordsSettingsAfterModalDismissFromViewController:
            (UIViewController*)baseViewController
                                                     showCancelButton:
                                                         (BOOL)
                                                             showCancelButton {
  if (!baseViewController) {
    // TODO(crbug.com/41352590): Don't pass base view controller through
    // dispatched command.
    baseViewController = self.currentInterface.viewController;
  }
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);

  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSavedPasswordsSettingsFromViewController:baseViewController
                                    showCancelButton:showCancelButton];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      savePasswordsControllerForBrowser:browser
                               delegate:self
                       showCancelButton:showCancelButton];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// Creates a `SettingsNavigationController` (if it doesn't already exist) and
// `PasswordCheckupCoordinator` for `referrer`, then starts the
// `PasswordCheckupCoordinator`.
- (void)startPasswordCheckupCoordinator:
    (password_manager::PasswordCheckReferrer)referrer {
  Browser* browser = self.mainInterface.browser;

  if (!self.settingsNavigationController) {
    self.settingsNavigationController =
        [SettingsNavigationController safetyCheckControllerForBrowser:browser
                                                             delegate:self
                                                             referrer:referrer];
  }

  self.passwordCheckupCoordinator = [[PasswordCheckupCoordinator alloc]
      initWithBaseNavigationController:self.settingsNavigationController
                               browser:browser
                          reauthModule:nil
                              referrer:referrer];

  self.passwordCheckupCoordinator.delegate = self;

  [self.passwordCheckupCoordinator start];
}

// Shows the Incognito interstitial on top of `activeViewController`.
// Assumes the Incognito interstitial coordinator is currently not instantiated.
// Runs `completion` once the Incognito interstitial is presented.
- (void)showIncognitoInterstitialWithUrlLoadParams:
    (const UrlLoadParams&)urlLoadParams {
  DCHECK(self.incognitoInterstitialCoordinator == nil);
  self.incognitoInterstitialCoordinator =
      [[IncognitoInterstitialCoordinator alloc]
          initWithBaseViewController:self.activeViewController
                             browser:self.currentInterface.browser];
  self.incognitoInterstitialCoordinator.delegate = self;
  self.incognitoInterstitialCoordinator.tabOpener = self;
  self.incognitoInterstitialCoordinator.urlLoadParams = urlLoadParams;
  [self.incognitoInterstitialCoordinator start];
}

// A sink for profileState:didTransitionFromInitStage: and
// sceneState:transitionedToActivationLevel: events.
//
// Discussion: the scene controller cares both about the profile and the scene
// init stages. This method is called from both observer callbacks and allows
// to handle all the transitions in one place.
- (void)transitionToSceneActivationLevel:(SceneActivationLevel)level
                        profileInitStage:(ProfileInitStage)profileInitStage {
  // Update `backgroundedSinceLastActivated` and, if the scene has just been
  // activated, mark its state before the current activation for future use.
  BOOL transitionedToForegroundActiveFromBackground =
      level == SceneActivationLevelForegroundActive &&
      self.backgroundedSinceLastActivated;
  if (level <= SceneActivationLevelBackground) {
    self.backgroundedSinceLastActivated = YES;
  } else if (level == SceneActivationLevelForegroundActive) {
    self.backgroundedSinceLastActivated = NO;
  }

  if (level == SceneActivationLevelDisconnected) {
    //  The scene may become disconnected at any time. In that case, any UI that
    //  was already set-up should be torn down.
    [self teardownUI];
  }
  if (profileInitStage < ProfileInitStage::kUIReady) {
    // Nothing else per-scene should happen before the app completes the global
    // setup, like executing Safe mode, or creating the main Profile.
    return;
  }

  BOOL initializingUIInColdStart =
      level > SceneActivationLevelBackground && !self.sceneState.UIEnabled;
  if (initializingUIInColdStart) {
    [self initializeUI];
    // Add the scene to the list of connected scene, to restore in case of
    // crashes.
    [[PreviousSessionInfo sharedInstance]
        addSceneSessionID:self.sceneState.sceneSessionID];
  }

  // When the scene transitions to inactive (such as when it's being shown in
  // the OS app-switcher), update the title for display on iPadOS.
  if (level == SceneActivationLevelForegroundInactive) {
    self.sceneState.scene.title = [self displayTitleForAppSwitcher];
  }

  if (level == SceneActivationLevelForegroundActive &&
      profileInitStage == ProfileInitStage::kFinal) {
    [self tryPresentSigninUpgradePromo];
    [self handleExternalIntents];

    if (!initializingUIInColdStart &&
        transitionedToForegroundActiveFromBackground &&
        self.mainCoordinator.isTabGridActive &&
        [self shouldOpenNTPTabOnActivationOfBrowser:self.currentInterface
                                                        .browser]) {
      DCHECK(!self.activatingBrowser);
      [self beginActivatingBrowser:self.mainInterface.browser focusOmnibox:NO];

      OpenNewTabCommand* command = [OpenNewTabCommand commandWithIncognito:NO];
      command.userInitiated = NO;
      Browser* browser = self.currentInterface.browser;
      id<ApplicationCommands> applicationHandler = HandlerForProtocol(
          browser->GetCommandDispatcher(), ApplicationCommands);
      [applicationHandler openURLInNewTab:command];
      [self finishActivatingBrowserDismissingTabSwitcher];
    }
  }
  if (level == SceneActivationLevelBackground) {
    [self recordWindowCreationForSceneState:self.sceneState];
  }

  if (self.sceneState.UIEnabled && level <= SceneActivationLevelDisconnected) {
    if (base::ios::IsMultipleScenesSupported()) {
      // If Multiple scenes are not supported, the session shouldn't be
      // removed as it can be used for normal restoration.
      [[PreviousSessionInfo sharedInstance]
          removeSceneSessionID:self.sceneState.sceneSessionID];
    }
  }
}

- (void)initializeUI {
  if (self.sceneState.UIEnabled) {
    return;
  }

  [self startUpChromeUI];
  self.sceneState.UIEnabled = YES;
}

// Starts up a single chrome window and its UI.
- (void)startUpChromeUI {
  DCHECK(!self.browserViewWrangler);
  DCHECK(_sceneURLLoadingService.get());
  DCHECK(self.sceneState.profileState.profile);

  SceneState* sceneState = self.sceneState;
  ProfileIOS* profile = sceneState.profileState.profile;

  self.browserViewWrangler =
      [[BrowserViewWrangler alloc] initWithProfile:profile
                                        sceneState:sceneState
                               applicationEndpoint:self
                                  settingsEndpoint:self];

  // Create and start the BVC.
  [self.browserViewWrangler createMainCoordinatorAndInterface];
  Browser* mainBrowser = self.browserViewWrangler.mainInterface.browser;

  PromosManager* promosManager = PromosManagerFactory::GetForProfile(profile);

  DefaultBrowserPromoSceneAgent* defaultBrowserAgent =
      [[DefaultBrowserPromoSceneAgent alloc] init];
  defaultBrowserAgent.promosManager = promosManager;
  [sceneState addAgent:defaultBrowserAgent];
  [sceneState
      addAgent:[[NonModalDefaultBrowserPromoSchedulerSceneAgent alloc] init]];

  // Add scene agents that require CommandDispatcher.
  CommandDispatcher* mainCommandDispatcher =
      mainBrowser->GetCommandDispatcher();
  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(mainCommandDispatcher, ApplicationCommands);
  id<PolicyChangeCommands> policyChangeCommandsHandler =
      HandlerForProtocol(mainCommandDispatcher, PolicyChangeCommands);

  [sceneState
      addAgent:[[SigninPolicySceneAgent alloc]
                       initWithSceneUIProvider:self
                    applicationCommandsHandler:applicationCommandsHandler
                   policyChangeCommandsHandler:policyChangeCommandsHandler]];

  PrefService* prefService = profile->GetPrefs();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);

  policy::UserCloudPolicyManager* userPolicyManager =
      profile->GetUserCloudPolicyManager();
  if (IsUserPolicyNotificationNeeded(authService, prefService,
                                     userPolicyManager)) {
    policy::UserPolicySigninService* userPolicyService =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile);
    [sceneState
        addAgent:[[UserPolicySceneAgent alloc]
                        initWithSceneUIProvider:self
                                    authService:authService
                     applicationCommandsHandler:applicationCommandsHandler
                                    prefService:prefService
                                    mainBrowser:mainBrowser
                                  policyService:userPolicyService
                              userPolicyManager:userPolicyManager]];
  }

  enterprise_idle::IdleService* idleService =
      enterprise_idle::IdleServiceFactory::GetForProfile(profile);
  id<SnackbarCommands> snackbarCommandsHandler =
      static_cast<id<SnackbarCommands>>(mainCommandDispatcher);

  [sceneState addAgent:[[IdleTimeoutPolicySceneAgent alloc]
                              initWithSceneUIProvider:self
                           applicationCommandsHandler:applicationCommandsHandler
                              snackbarCommandsHandler:snackbarCommandsHandler
                                          idleService:idleService
                                          mainBrowser:mainBrowser]];

  // Now that the main browser's command dispatcher is created and the newly
  // started UI coordinators have registered with it, inject it into the
  // PolicyWatcherBrowserAgent so it can start monitoring UI-impacting policy
  // changes.
  PolicyWatcherBrowserAgent* policyWatcherAgent =
      PolicyWatcherBrowserAgent::FromBrowser(self.mainInterface.browser);
  _policyWatcherObserver = std::make_unique<base::ScopedObservation<
      PolicyWatcherBrowserAgent, PolicyWatcherBrowserAgentObserverBridge>>(
      _policyWatcherObserverBridge.get());
  _policyWatcherObserver->Observe(policyWatcherAgent);
  policyWatcherAgent->Initialize(policyChangeCommandsHandler);

  self.screenshotDelegate = [[ScreenshotDelegate alloc]
      initWithBrowserProviderInterface:self.browserViewWrangler];
  [sceneState.scene.screenshotService setDelegate:self.screenshotDelegate];

  [self activateBVCAndMakeCurrentBVCPrimary];
  [self.browserViewWrangler loadSession];
  [self createInitialUI:[self initialUIMode]];

  // Make sure the GeolocationManager is created to observe permission events.
  [GeolocationManager sharedInstance];

  if (ShouldPromoManagerDisplayPromos()) {
    [sceneState addAgent:[[PromosManagerSceneAgent alloc]
                             initWithCommandDispatcher:mainCommandDispatcher]];
  }

  if (IsAppStoreRatingEnabled()) {
    [sceneState addAgent:[[AppStoreRatingSceneAgent alloc]
                             initWithPromosManager:promosManager]];
  }

  [sceneState addAgent:[[WhatsNewSceneAgent alloc]
                           initWithPromosManager:promosManager]];

  // Do not gate by feature flag so it can run for enabled -> disabled
  // scenarios.
  [sceneState addAgent:[[CredentialProviderPromoSceneAgent alloc]
                           initWithPromosManager:promosManager]];
}

// Determines the mode (normal or incognito) the initial UI should be in.
- (ApplicationMode)initialUIMode {
  // When only incognito mode is available.
  if ([self isIncognitoForced]) {
    return ApplicationMode::INCOGNITO;
  }

  // When only incognito mode is disabled.
  if ([self isIncognitoDisabled]) {
    return ApplicationMode::NORMAL;
  }

  // Check if the UI is being created from an intent; if it is, open in the
  // correct mode for that activity. Because all activities must be in the same
  // mode, as soon as any activity reports being in incognito, switch to that
  // mode.
  for (NSUserActivity* activity in self.sceneState.connectionOptions
           .userActivities) {
    if (ActivityIsTabMove(activity)) {
      return GetIncognitoFromTabMoveActivity(activity)
                 ? ApplicationMode::INCOGNITO
                 : ApplicationMode::NORMAL;
    }
  }

  // Launch in the mode that matches the state of the scene when the application
  // was terminated. If the scene was showing the incognito UI, but there are
  // no incognito tabs open (e.g. the tab switcher was active and user closed
  // the last tab), then instead show the regular UI.

  if (self.sceneState.incognitoContentVisible &&
      !self.incognitoInterface.browser->GetWebStateList()->empty()) {
    return ApplicationMode::INCOGNITO;
  }

  // In all other cases, default to normal mode.
  return ApplicationMode::NORMAL;
}

// Creates and displays the initial UI in `launchMode`, performing other
// setup and configuration as needed.
- (void)createInitialUI:(ApplicationMode)launchMode {
  // Set the Scene application URL loader on the URL loading browser interface
  // for the regular and incognito interfaces. This will lazily instantiate the
  // incognito interface if it isn't already created.
  UrlLoadingBrowserAgent::FromBrowser(self.mainInterface.browser)
      ->SetSceneService(_sceneURLLoadingService.get());
  UrlLoadingBrowserAgent::FromBrowser(self.incognitoInterface.browser)
      ->SetSceneService(_sceneURLLoadingService.get());
  // Observe the web state lists for both browsers.
  _incognitoWebStateObserver = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>(
      _webStateListForwardingObserver.get());
  _incognitoWebStateObserver->Observe(
      self.incognitoInterface.browser->GetWebStateList());
  _mainWebStateObserver = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>(
      _webStateListForwardingObserver.get());
  _mainWebStateObserver->Observe(self.mainInterface.browser->GetWebStateList());

  if (base::FeatureList::IsEnabled(
          kMakeKeyAndVisibleBeforeMainCoordinatorStart)) {
    [self.sceneState.window makeKeyAndVisible];
  }

  // Lazy init of mainCoordinator.
  [self.mainCoordinator start];

  if (!base::FeatureList::IsEnabled(
          kMakeKeyAndVisibleBeforeMainCoordinatorStart)) {
    // Enables UI initializations to query the keyWindow's size. Do this after
    // `mainCoordinator start` as it sets self.window.rootViewController to work
    // around crbug.com/850387, causing a flicker if -makeKeyAndVisible has been
    // called.
    [self.sceneState.window makeKeyAndVisible];
  }

  if (!self.sceneState.appState.startupInformation.isFirstRun) {
    [self reconcileEulaAsAccepted];
  }

  Browser* browser = (launchMode == ApplicationMode::INCOGNITO)
                         ? self.incognitoInterface.browser
                         : self.mainInterface.browser;

  // Inject a NTP before setting the interface, which will trigger a load of
  // the current webState.
  if (self.sceneState.appState.postCrashAction ==
      PostCrashAction::kShowNTPWithReturnToTab) {
    InjectNTP(browser);
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  int tabCountToAdd =
      [[NSUserDefaults standardUserDefaults] integerForKey:kAddLotsOfTabs];
  // Also check an environment variable for some other test environments which
  // expect a minimum number of tabs.
  if (tabCountToAdd == 0) {
    tabCountToAdd = [[NSProcessInfo.processInfo.environment
        objectForKey:@"MINIMUM_TAB_COUNT"] intValue];
    tabCountToAdd -= browser->GetWebStateList()->count();
  }
  if (tabCountToAdd > 0) {
    [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:kAddLotsOfTabs];
    InjectUnrealizedWebStates(browser, tabCountToAdd);
  }
#endif

  if (launchMode == ApplicationMode::INCOGNITO) {
    [self setCurrentInterfaceForMode:ApplicationMode::INCOGNITO];
  } else {
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  }

  // Figure out what UI to show initially.

  if (self.mainCoordinator.isTabGridActive) {
    DCHECK(!self.activatingBrowser);
    [self beginActivatingBrowser:self.mainInterface.browser focusOmnibox:NO];
    [self finishActivatingBrowserDismissingTabSwitcher];
  }

  // If this web state list should have an NTP created when it activates, then
  // create that tab.
  if ([self shouldOpenNTPTabOnActivationOfBrowser:browser]) {
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithIncognito:self.currentInterface.incognito];
    command.userInitiated = NO;
    Browser* currentBrowser = self.currentInterface.browser;
    id<ApplicationCommands> applicationHandler = HandlerForProtocol(
        currentBrowser->GetCommandDispatcher(), ApplicationCommands);
    [applicationHandler openURLInNewTab:command];
  }
}

- (void)teardownUI {
  // The UI should be stopped before the models they observe are stopped.
  // SigninCoordinator teardown is performed by the `signinCompletion` on
  // termination of async events, do not add additional teardown here.
  [self.signinCoordinator
      interruptWithAction:SigninCoordinatorInterrupt::UIShutdownNoDismiss
               completion:nil];
  // `self.signinCoordinator.signinCompletion()` was called in the interrupt
  // method. Therefore now `self.signinCoordinator` is now stopped, and
  // `self.signinCoordinator` is now nil.
  DCHECK(!self.signinCoordinator)
      << base::SysNSStringToUTF8([self.signinCoordinator description]);

  [self.historyCoordinator stop];
  self.historyCoordinator = nil;

  // Force close the settings if open. This gives Settings the opportunity to
  // unregister observers and destroy C++ objects before the application is
  // shut down without depending on non-deterministic call to -dealloc.
  [self settingsWasDismissed];

  [_mainCoordinator stop];
  _mainCoordinator = nil;

  _incognitoWebStateObserver.reset();
  _mainWebStateObserver.reset();
  _policyWatcherObserver.reset();

  // TODO(crbug.com/40778288): Consider moving this at the beginning of
  // teardownUI to indicate that the UI is about to be torn down and that the
  // dependencies depending on the browser UI models has to be cleaned up
  // agent).
  self.sceneState.UIEnabled = NO;

  [[SessionSavingSceneAgent agentFromScene:self.sceneState]
      saveSessionsIfNeeded];
  [self.browserViewWrangler shutdown];
  self.browserViewWrangler = nil;

  [self.sceneState.profileState removeObserver:self];
  _sceneURLLoadingService.reset();
}

// Formats string for display on iPadOS application switcher with the
// domain of the foreground tab and the tab count. Assumes the scene is
// visible. Will return nil if there are no tabs.
- (NSString*)displayTitleForAppSwitcher {
  DCHECK(self.currentInterface.browser);
  web::WebState* webState =
      self.currentInterface.browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    return nil;
  }

  // At this point there is at least one tab.
  int numberOfTabs = self.currentInterface.browser->GetWebStateList()->count();
  DCHECK(numberOfTabs > 0);
  GURL url = webState->GetVisibleURL();
  std::u16string urlText = url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlTrimAfterHost,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  std::u16string pattern =
      l10n_util::GetStringUTF16(IDS_IOS_APP_SWITCHER_SCENE_TITLE);
  std::u16string formattedTitle =
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          pattern, "domain", urlText, "count", numberOfTabs - 1);
  return base::SysUTF16ToNSString(formattedTitle);
}

// If users request to open tab or search and Chrome is not opened in the mode
// they expected, show a toast to clarify that the expected mode is not
// available.
- (void)showToastWhenOpenExternalIntentInUnexpectedMode {
  id<SnackbarCommands> handler = HandlerForProtocol(
      self.mainInterface.browser->GetCommandDispatcher(), SnackbarCommands);
  BOOL inIncognitoMode = [self isIncognitoForced];

  UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUIManagementURL));
  params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  ProceduralBlock moreAction = ^{
    [self dismissModalsAndMaybeOpenSelectedTabInMode:
              inIncognitoMode ? ApplicationModeForTabOpening::INCOGNITO
                              : ApplicationModeForTabOpening::NORMAL
                                   withUrlLoadParams:params
                                      dismissOmnibox:YES
                                          completion:nil];
  };

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = moreAction;
  action.title = l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_MORE_BUTTON);
  action.accessibilityIdentifier =
      l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_MORE_BUTTON);

  NSString* text =
      inIncognitoMode
          ? l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_FORCED)
          : l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_INCOGNITO_DISABLED);

  MDCSnackbarMessage* message = CreateSnackbarMessage(text);
  message.action = action;

  [handler showSnackbarMessage:message
                withHapticType:UINotificationFeedbackTypeError];
}

- (BOOL)isIncognitoDisabled {
  return IsIncognitoModeDisabled(
      self.mainInterface.browser->GetProfile()->GetPrefs());
}

// YES if incognito mode is forced by enterprise policy.
- (BOOL)isIncognitoForced {
  return IsIncognitoModeForced(
      self.incognitoInterface.browser->GetProfile()->GetPrefs());
}

// Returns 'YES' if the tabID from the given `activity` is valid.
- (BOOL)isTabActivityValid:(NSUserActivity*)activity {
  web::WebStateID tabID = GetTabIDFromActivity(activity);

  ProfileIOS* profile = self.currentInterface.profile;
  BrowserList* browserList = BrowserListFactory::GetForProfile(profile);
  const BrowserList::BrowserType browser_types =
      self.currentInterface.incognito
          ? BrowserList::BrowserType::kIncognito
          : BrowserList::BrowserType::kRegularAndInactive;
  std::set<Browser*> browsers = browserList->BrowsersOfType(browser_types);

  BrowserAndIndex tabInfo = FindBrowserAndIndex(tabID, browsers);

  return tabInfo.tab_index != WebStateList::kInvalidIndex;
}

// Sets a LocalState pref marking the TOS EULA as accepted.
// If this function is called, the EULA flag is not set but the FRE was not
// displayed.
// This can only happen if the EULA flag has not been set correctly on a
// previous session.
- (void)reconcileEulaAsAccepted {
  static dispatch_once_t once_token = 0;
  dispatch_once(&once_token, ^{
    PrefService* prefs = GetApplicationContext()->GetLocalState();
    if (!FirstRun::IsChromeFirstRun() &&
        !prefs->GetBoolean(prefs::kEulaAccepted)) {
      prefs->SetBoolean(prefs::kEulaAccepted, true);
      prefs->CommitPendingWrite();
      base::UmaHistogramBoolean("IOS.ReconcileEULAPref", true);
    }
  });
}

// Returns YES if the sign-in upgrade promo should be presented.
- (BOOL)shouldPresentSigninUpgradePromo {
  if (![self isTabAvailableToPresentViewController]) {
    return NO;
  }
  if (!signin::ShouldPresentUserSigninUpgrade(
          self.sceneState.browserProviderInterface.mainBrowserProvider.browser
              ->GetProfile(),
          version_info::GetVersion())) {
    return NO;
  }
  // Don't show the promo in Incognito mode.
  if (self.currentInterface == self.incognitoInterface) {
    return NO;
  }
  // Don't show promos if the app was launched from a URL.
  if (self.startupParameters) {
    return NO;
  }
  // Don't show the promo if the window is not active.
  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    return NO;
  }
  // Don't show the promo if already presented.
  if (self.sceneState.appState.signinUpgradePromoPresentedOnce) {
    return NO;
  }
  return YES;
}

// Presents the sign-in upgrade promo.
- (void)tryPresentSigninUpgradePromo {
  // It is possible during a slow asynchronous call that the user changes their
  // state so as to no longer be eligible for sign-in promos. Return early in
  // this case.
  if (![self shouldPresentSigninUpgradePromo]) {
    return;
  }
  self.sceneState.appState.signinUpgradePromoPresentedOnce = YES;
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  Browser* browser = self.mainInterface.browser;
  self.signinCoordinator = [SigninCoordinator
      upgradeSigninPromoCoordinatorWithBaseViewController:self.mainInterface
                                                              .viewController
                                                  browser:browser];
  [self startSigninCoordinatorWithCompletion:nil];
}

- (BOOL)canHandleIntents {
  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    return NO;
  }

  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    return NO;
  }

  if (self.sceneState.presentingModalOverlay) {
    return NO;
  }

  if (IsSigninForcedByPolicy()) {
    if (self.signinCoordinator) {
      // Return NO because intents cannot be handled when using
      // `self.signinCoordinator` for the forced sign-in prompt.
      return NO;
    }
    if (![self isSignedIn]) {
      // Return NO if the forced sign-in policy is enabled while the browser is
      // signed out because intent can only be processed when the browser is
      // signed-in in that case. This condition may be reached at startup before
      // `self.signinCoordinator` is set to show the forced sign-in prompt.
      return NO;
    }
  }

  return YES;
}

- (BOOL)isSignedIn {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(
          self.sceneState.browserProviderInterface.mainBrowserProvider.browser
              ->GetProfile());
  DCHECK(authenticationService);
  DCHECK(authenticationService->initialized());

  return authenticationService->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin);
}

#pragma mark - ApplicationCommands

- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion {
  [self dismissModalDialogsWithCompletion:completion dismissOmnibox:YES];
}

- (void)showHistory {
  CHECK(!self.currentInterface.incognito)
      << "Current interface is incognito and should NOT show history. Call "
         "this on regular interface.";
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.currentInterface.viewController
                         browser:self.mainInterface.browser];
  self.historyCoordinator.loadStrategy = UrlLoadStrategy::NORMAL;
  self.historyCoordinator.delegate = self;
  [self.historyCoordinator start];
}

// Opens an url from a link in the settings UI.
- (void)closeSettingsUIAndOpenURL:(OpenNewTabCommand*)command {
  DCHECK([command fromChrome]);
  UrlLoadParams params = UrlLoadParams::InNewTab([command URL]);
  params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  ProceduralBlock completion = ^{
    ApplicationModeForTabOpening mode =
        [self isIncognitoForced] ? ApplicationModeForTabOpening::INCOGNITO
                                 : ApplicationModeForTabOpening::NORMAL;
    [self dismissModalsAndMaybeOpenSelectedTabInMode:mode
                                   withUrlLoadParams:params
                                      dismissOmnibox:YES
                                          completion:nil];
  };
  [self closePresentedViews:YES completion:completion];
}

- (void)closeSettingsUI {
  [self closePresentedViews:YES completion:nullptr];
}

- (void)prepareTabSwitcher {
  web::WebState* currentWebState =
      self.currentInterface.browser->GetWebStateList()->GetActiveWebState();
  if (currentWebState) {
    SnapshotTabHelper::FromWebState(currentWebState)
        ->UpdateSnapshotWithCallback(nil);
  }
}

- (void)displayTabGridInMode:(TabGridOpeningMode)mode {
  if (self.mainCoordinator.isTabGridActive) {
    return;
  }

  if (!self.isProcessingVoiceSearchCommand) {
    BOOL incognito = self.currentInterface.incognito;
    if (mode == TabGridOpeningMode::kRegular && incognito) {
      [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
    } else if (mode == TabGridOpeningMode::kIncognito && !incognito) {
      [self setCurrentInterfaceForMode:ApplicationMode::INCOGNITO];
    }

    [self showTabSwitcher];
    self.isProcessingTabSwitcherCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     self.isProcessingTabSwitcherCommand = NO;
                   });
  }
}

- (void)showPrivacySettingsFromViewController:
    (UIViewController*)baseViewController {
  if (self.settingsNavigationController) {
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController privacyControllerForBrowser:browser
                                                       delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showReportAnIssueFromViewController:
            (UIViewController*)baseViewController
                                     sender:(UserFeedbackSender)sender {
  [self showReportAnIssueFromViewController:baseViewController
                                     sender:sender
                        specificProductData:nil];
}

- (void)
    showReportAnIssueFromViewController:(UIViewController*)baseViewController
                                 sender:(UserFeedbackSender)sender
                    specificProductData:(NSDictionary<NSString*, NSString*>*)
                                            specificProductData {
  DCHECK(baseViewController);
  // This dispatch is necessary to give enough time for the tools menu to
  // disappear before taking a screenshot.
  __weak SceneController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    // Set the delay timeout to capture about 85% of users (approx. 2 seconds),
    // see Signin.ListFamilyMembersRequest.OverallLatency.
    [weakSelf presentReportAnIssueViewController:baseViewController
                                          sender:sender
                             specificProductData:specificProductData
                                         timeout:base::Seconds(2)
                                      completion:base::DoNothing()];
  });
}

using UserFeedbackDataCallback =
    base::RepeatingCallback<void(UserFeedbackData*)>;

- (void)presentReportAnIssueViewController:(UIViewController*)baseViewController
                                    sender:(UserFeedbackSender)sender
                       specificProductData:(NSDictionary<NSString*, NSString*>*)
                                               specificProductData
                                   timeout:(base::TimeDelta)timeout
                                completion:
                                    (UserFeedbackDataCallback)completion {
  UserFeedbackData* userFeedbackData =
      [self createUserFeedbackDataForSender:sender
                        specificProductData:specificProductData];
  [self presentReportAnIssueViewController:baseViewController
                                    sender:sender
                          userFeedbackData:userFeedbackData
                                   timeout:timeout
                                completion:std::move(completion)];
}

- (void)presentReportAnIssueViewController:(UIViewController*)baseViewController
                                    sender:(UserFeedbackSender)sender
                          userFeedbackData:(UserFeedbackData*)data
                                   timeout:(base::TimeDelta)timeout
                                completion:
                                    (UserFeedbackDataCallback)completion {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (self.settingsNavigationController) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(self.mainInterface.profile);
  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  // Retrieves the Family Link member role for the signed-in account and
  // populates the corresponding `UserFeedbackData` property.
  if (!primary_account.IsEmpty()) {
    __weak SceneController* weakSelf = self;
    _family_members_fetcher = supervised_user::FetchListFamilyMembers(
        *identity_manager,
        self.mainInterface.profile->GetSharedURLLoaderFactory(),
        base::BindOnce(&OnListFamilyMembersResponse, primary_account.gaia, data)
            .Then(base::BindOnce(^{
              [weakSelf presentUserFeedbackViewController:baseViewController
                                     withUserFeedbackData:data
                                 cancelFamilyMembersFetch:NO
                                               completion:completion];
            })));

    // Timeout the request to list family members.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf presentUserFeedbackViewController:baseViewController
                                 withUserFeedbackData:data
                             cancelFamilyMembersFetch:YES
                                           completion:completion];
        }),
        timeout);
    return;
  }

  [self presentUserFeedbackViewController:baseViewController
                     withUserFeedbackData:data
                 cancelFamilyMembersFetch:NO
                               completion:completion];
}

- (void)presentUserFeedbackViewController:(UIViewController*)baseViewController
                     withUserFeedbackData:(UserFeedbackData*)data
                 cancelFamilyMembersFetch:(BOOL)cancelFamilyMembersFetch
                               completion:(UserFeedbackDataCallback)completion {
  // Cancel any list family member requests in progress.
  if (cancelFamilyMembersFetch) {
    _family_members_fetcher.reset();
  }

  Browser* browser = self.mainInterface.browser;

  id<ApplicationCommands> handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);

  if (ios::provider::CanUseStartUserFeedbackFlow()) {
    UserFeedbackConfiguration* configuration =
        [[UserFeedbackConfiguration alloc] init];
    configuration.data = data;
    configuration.handler = handler;
    configuration.singleSignOnService =
        GetApplicationContext()->GetSingleSignOnService();

    NSError* error;
    ios::provider::StartUserFeedbackFlow(configuration, baseViewController,
                                         &error);
    UMA_HISTOGRAM_BOOLEAN("IOS.FeedbackKit.UserFlowStartedSuccess",
                          error == nil);
  } else {
    self.settingsNavigationController =
        [SettingsNavigationController userFeedbackControllerForBrowser:browser
                                                              delegate:self
                                                      userFeedbackData:data];
    [baseViewController presentViewController:self.settingsNavigationController
                                     animated:YES
                                   completion:nil];
  }
  std::move(completion).Run(data);
}

- (UserFeedbackData*)createUserFeedbackDataForSender:(UserFeedbackSender)sender
                                 specificProductData:
                                     (NSDictionary<NSString*, NSString*>*)
                                         specificProductData {
  UserFeedbackData* data = [[UserFeedbackData alloc] init];
  data.origin = sender;
  data.currentPageIsIncognito = self.currentInterface.incognito;

  CGFloat scale = 0.0;
  if (self.mainCoordinator.isTabGridActive) {
    // For screenshots of the tab switcher we need to use a scale of 1.0 to
    // avoid spending too much time since the tab switcher can have lots of
    // subviews.
    scale = 1.0;
  }

  UIView* lastView = self.mainCoordinator.activeViewController.view;
  DCHECK(lastView);
  data.currentPageScreenshot = CaptureView(lastView, scale);

  ProfileIOS* profile = self.currentInterface.profile;
  if (profile->IsOffTheRecord()) {
    data.currentPageIsIncognito = YES;
  } else {
    data.currentPageIsIncognito = NO;
  }

  data.productSpecificData = specificProductData;
  return data;
}

- (void)openURLInNewTab:(OpenNewTabCommand*)command {
  if (command.inIncognito) {
    IncognitoReauthSceneAgent* reauthAgent =
        [IncognitoReauthSceneAgent agentFromScene:self.sceneState];
    if (reauthAgent.authenticationRequired) {
      __weak SceneController* weakSelf = self;
      [reauthAgent
          authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
            if (success) {
              [weakSelf openURLInNewTab:command];
            }
          }];
      return;
    }
  }

  UrlLoadParams params =
      UrlLoadParams::InNewTab(command.URL, command.virtualURL);
  params.SetInBackground(command.inBackground);
  params.web_params.referrer = command.referrer;
  params.in_incognito = command.inIncognito;
  params.append_to = command.appendTo;
  params.origin_point = command.originPoint;
  params.from_chrome = command.fromChrome;
  params.user_initiated = command.userInitiated;
  params.should_focus_omnibox = command.shouldFocusOmnibox;
  params.inherit_opener = !command.inBackground;
  _sceneURLLoadingService->LoadUrlInNewTab(params);
}

// TODO(crbug.com/41352590) : Do not pass `baseViewController` through
// dispatcher.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController {
  // Calling this method when there is a signinCoordinator alive is incorrect
  // as there should not be 2 signinCoordinators alive at the same time (note
  // that allocating the second one will dealloc the first and this crashes in
  // various ways).
  if (command.skipIfUINotAvaible &&
      (baseViewController.presentedViewController ||
       ![self isTabAvailableToPresentViewController])) {
    // Make sure the UI is available to present the sign-in view.
    return;
  }
  if (self.signinCoordinator) {
    // As of M121, the CHECK bellow is known to fire in various cases. The goal
    // of the histograms below is to detect the number of incorrect cases and
    // for which of the access points they are triggered.
    base::UmaHistogramEnumeration(
        "Signin.ShowSigninCoordinatorWhenAlreadyPresent.NewAccessPoint",
        command.accessPoint, signin_metrics::AccessPoint::ACCESS_POINT_MAX);
    base::UmaHistogramEnumeration(
        "Signin.ShowSigninCoordinatorWhenAlreadyPresent.OldAccessPoint",
        self.signinCoordinator.accessPoint,
        signin_metrics::AccessPoint::ACCESS_POINT_MAX);
    // The goal of this histogram is to understand if the issue is related to
    // a double tap (duration less than 1s), or if `self.signinCoordinator`
    // is not visible anymore on the screen (duration more than 1s).
    const base::TimeDelta duration =
        base::TimeTicks::Now() - self.signinCoordinator.creationTimeTicks;
    UmaHistogramTimes("Signin.ShowSigninCoordinatorWhenAlreadyPresent."
                      "DurationBetweenTwoSigninCoordinatorCreation",
                      duration);
  }
  // TODO(crbug.com/40071586): Change this to a CHECK once this invariant is
  // correct.
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  Browser* mainBrowser = self.mainInterface.browser;

  switch (command.operation) {
    case AuthenticationOperation::kPrimaryAccountReauth:
      self.signinCoordinator = [SigninCoordinator
          primaryAccountReauthCoordinatorWithBaseViewController:
              baseViewController
                                                        browser:mainBrowser
                                                    accessPoint:command
                                                                    .accessPoint
                                                    promoAction:
                                                        command.promoAction];
      break;
    case AuthenticationOperation::kResignin:
      self.signinCoordinator = [SigninCoordinator
          signinAndSyncReauthCoordinatorWithBaseViewController:
              baseViewController
                                                       browser:mainBrowser
                                                   accessPoint:command
                                                                   .accessPoint
                                                   promoAction:
                                                       command.promoAction];
      break;
    case AuthenticationOperation::kSigninOnly:
      self.signinCoordinator = [SigninCoordinator
          consistencyPromoSigninCoordinatorWithBaseViewController:
              baseViewController
                                                          browser:mainBrowser
                                                      accessPoint:
                                                          command.accessPoint];
      break;
    case AuthenticationOperation::kAddAccount:
      self.signinCoordinator = [SigninCoordinator
          addAccountCoordinatorWithBaseViewController:baseViewController
                                              browser:mainBrowser
                                          accessPoint:command.accessPoint];
      break;
    case AuthenticationOperation::kForcedSigninAndSync:
      self.signinCoordinator = [SigninCoordinator
          forcedSigninCoordinatorWithBaseViewController:baseViewController
                                                browser:mainBrowser
                                            accessPoint:command.accessPoint];
      break;
    case AuthenticationOperation::kInstantSignin:
      self.signinCoordinator = [SigninCoordinator
          instantSigninCoordinatorWithBaseViewController:baseViewController
                                                 browser:mainBrowser
                                                identity:command.identity
                                             accessPoint:command.accessPoint
                                             promoAction:command.promoAction];
      break;
    case AuthenticationOperation::kSheetSigninAndHistorySync:
      self.signinCoordinator = [SigninCoordinator
          sheetSigninAndHistorySyncCoordinatorWithBaseViewController:
              baseViewController
                                                             browser:mainBrowser
                                                         accessPoint:
                                                             command.accessPoint
                                                         promoAction:
                                                             command
                                                                 .promoAction];
      break;
  }
  [self startSigninCoordinatorWithCompletion:command.callback];
}

- (void)
    switchAccountWithBaseViewController:(UIViewController*)baseViewController
                            newIdentity:(id<SystemIdentity>)newIdentity
                                   rect:(CGRect)rect
                         rectAnchorView:(UIView*)rectAnchorView
        viewWillBeDismissedAfterSignout:(BOOL)viewWillBeDismissedAfterSignout
                 userDecisionCompletion:(void (^)())userDecisionCompletion
                       signInCompletion:(ShowSigninCommandCompletionCallback)
                                            signInCompletion {
  UIViewController* mainViewController = viewWillBeDismissedAfterSignout
                                             ? self.mainInterface.viewController
                                             : baseViewController;
  self.signinCoordinator = [[AccountSwitchCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:self.mainInterface.browser
                     newIdentity:newIdentity
              mainViewController:mainViewController
                            rect:rect
          userDecisionCompletion:(void (^)())userDecisionCompletion
                  rectAnchorView:rectAnchorView];
  [self startSigninCoordinatorWithCompletion:signInCompletion];
}

- (void)
    showTrustedVaultReauthForFetchKeysFromViewController:
        (UIViewController*)viewController
                                        securityDomainID:
                                            (trusted_vault::SecurityDomainId)
                                                securityDomainID
                                                 trigger:
                                                     (syncer::
                                                          TrustedVaultUserActionTriggerForUMA)
                                                         trigger
                                             accessPoint:
                                                 (signin_metrics::AccessPoint)
                                                     accessPoint {
  [self
      showTrustedVaultDialogFromViewController:viewController
                                        intent:
                                            SigninTrustedVaultDialogIntentFetchKeys
                              securityDomainID:securityDomainID
                                       trigger:trigger
                                   accessPoint:accessPoint];
}

- (void)
    showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
        (UIViewController*)viewController
                                                     securityDomainID:
                                                         (trusted_vault::
                                                              SecurityDomainId)
                                                             securityDomainID
                                                              trigger:
                                                                  (syncer::
                                                                       TrustedVaultUserActionTriggerForUMA)
                                                                      trigger
                                                          accessPoint:
                                                              (signin_metrics::
                                                                   AccessPoint)
                                                                  accessPoint {
  [self
      showTrustedVaultDialogFromViewController:viewController
                                        intent:
                                            SigninTrustedVaultDialogIntentDegradedRecoverability
                              securityDomainID:securityDomainID
                                       trigger:trigger
                                   accessPoint:accessPoint];
}

- (void)showWebSigninPromoFromViewController:
            (UIViewController*)baseViewController
                                         URL:(const GURL&)url {
  // Do not display the web sign-in promo if there is any UI on the screen.
  if (baseViewController.presentedViewController ||
      ![self isTabAvailableToPresentViewController]) {
    return;
  }
  if (!signin::ShouldPresentWebSignin(self.mainInterface.profile)) {
    return;
  }
  self.signinCoordinator = [SigninCoordinator
      consistencyPromoSigninCoordinatorWithBaseViewController:baseViewController
                                                      browser:self.mainInterface
                                                                  .browser
                                                  accessPoint:
                                                      signin_metrics::AccessPoint::
                                                          ACCESS_POINT_WEB_SIGNIN];
  if (!self.signinCoordinator) {
    return;
  }
  __weak SceneController* weakSelf = self;
  // Copy the URL so it can be safely captured in the block.
  GURL copiedURL = url;
  [self startSigninCoordinatorWithCompletion:^(
            SigninCoordinatorResult result,
            SigninCompletionInfo* completionInfo) {
    // If the sign-in is not successful or the scene controller is shut down do
    // not load the continuation URL.
    BOOL success = result == SigninCoordinatorResultSuccess;
    if (!success || !weakSelf) {
      return;
    }
    UrlLoadingBrowserAgent::FromBrowser(weakSelf.mainInterface.browser)
        ->Load(UrlLoadParams::InCurrentTab(copiedURL));
  }];
}

- (void)showSigninAccountNotificationFromViewController:
    (UIViewController*)baseViewController {
  web::WebState* webState =
      self.mainInterface.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(webState);
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  DCHECK(infoBarManager);
  CommandDispatcher* dispatcher =
      self.mainInterface.browser->GetCommandDispatcher();
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  SigninNotificationInfoBarDelegate::Create(
      infoBarManager, self.mainInterface.browser->GetProfile(), settingsHandler,
      baseViewController);
}

- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible {
  self.sceneState.incognitoContentVisible = incognitoContentVisible;
}

- (void)startVoiceSearch {
  if (!self.isProcessingTabSwitcherCommand) {
    [self startVoiceSearchInCurrentBVC];
    self.isProcessingVoiceSearchCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     self.isProcessingVoiceSearchCommand = NO;
                   });
  }
}

- (void)showSettingsFromViewController:(UIViewController*)baseViewController {
  BOOL hasDefaultBrowserBlueDot = NO;

  Browser* browser = self.mainInterface.browser;
  if (browser) {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(
            browser->GetProfile());
    if (tracker) {
      hasDefaultBrowserBlueDot =
          ShouldTriggerDefaultBrowserHighlightFeature(tracker);
    }
  }

  if (hasDefaultBrowserBlueDot) {
    RecordDefaultBrowserBlueDotFirstDisplay();
  }

  [self showSettingsFromViewController:baseViewController
              hasDefaultBrowserBlueDot:hasDefaultBrowserBlueDot];
}

- (void)showSettingsFromViewController:(UIViewController*)baseViewController
              hasDefaultBrowserBlueDot:(BOOL)hasDefaultBrowserBlueDot {
  if (!baseViewController) {
    baseViewController = self.currentInterface.viewController;
  }

  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (self.settingsNavigationController) {
    DCHECK(self.settingsNavigationController.presentingViewController)
        << base::SysNSStringToUTF8(
               [self.settingsNavigationController.viewControllers description]);
    return;
  }
  [[DeferredInitializationRunner sharedInstance]
      runBlockIfNecessary:kPrefObserverInit];

  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController = [SettingsNavigationController
      mainSettingsControllerForBrowser:browser
                              delegate:self
              hasDefaultBrowserBlueDot:hasDefaultBrowserBlueDot];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showPriceTrackingNotificationsSettings {
  CHECK(!self.settingsNavigationController, base::NotFatalUntil::M134);
  CHECK(!self.signinCoordinator, base::NotFatalUntil::M134);
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      priceNotificationsControllerForBrowser:browser
                                    delegate:self];
  [self.currentInterface.viewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)openNewWindowWithActivity:(NSUserActivity*)userActivity {
  if (!base::ios::IsMultipleScenesSupported()) {
    return;  // silent no-op.
  }

  UISceneActivationRequestOptions* options =
      [[UISceneActivationRequestOptions alloc] init];
  options.requestingScene = self.sceneState.scene;

  if (self.mainInterface) {
    PrefService* prefs = self.mainInterface.profile->GetPrefs();
    if (IsIncognitoModeForced(prefs)) {
      userActivity = AdaptUserActivityToIncognito(userActivity, true);
    } else if (IsIncognitoModeDisabled(prefs)) {
      userActivity = AdaptUserActivityToIncognito(userActivity, false);
    }

    [UIApplication.sharedApplication
        requestSceneSessionActivation:nil /* make a new scene */
                         userActivity:userActivity
                              options:options
                         errorHandler:nil];
  }
}

- (void)prepareToPresentModal:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock ensureNTP = ^{
    [weakSelf ensureNTP];
    completion();
  };
  if (self.mainCoordinator.isTabGridActive ||
      (self.currentInterface.incognito && ![self isIncognitoForced])) {
    [self closePresentedViews:YES
                   completion:^{
                     [weakSelf openNonIncognitoTab:ensureNTP];
                   }];
    return;
  }
  [self dismissModalDialogsWithCompletion:ensureNTP];
}

// Returns YES if the current Tab is available to present a view controller.
- (BOOL)isTabAvailableToPresentViewController {
  if (self.signinCoordinator) {
    return NO;
  }
  if (self.settingsNavigationController) {
    return NO;
  }
  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    return NO;
  }
  if (self.sceneState.appState.currentUIBlocker) {
    return NO;
  }
  if (self.mainCoordinator.isTabGridActive) {
    return NO;
  }
  return YES;
}

#pragma mark - SettingsCommands

// TODO(crbug.com/41352590) : Remove show settings from MainController.
- (void)showAccountsSettingsFromViewController:
            (UIViewController*)baseViewController
                          skipIfUINotAvailable:(BOOL)skipIfUINotAvailable {
  if (skipIfUINotAvailable && (baseViewController.presentedViewController ||
                               ![self isTabAvailableToPresentViewController])) {
    return;
  }
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (!baseViewController) {
    DCHECK_EQ(self.currentInterface.viewController,
              self.mainCoordinator.activeViewController);
    baseViewController = self.currentInterface.viewController;
  }

  if (self.currentInterface.incognito) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showAccountsSettingsFromViewController:baseViewController
                          skipIfUINotAvailable:NO];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController accountsControllerForBrowser:browser
                                                        delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Remove Google services settings from
// MainController.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (!baseViewController) {
    DCHECK_EQ(self.currentInterface.viewController,
              self.mainCoordinator.activeViewController);
    baseViewController = self.currentInterface.viewController;
  }

  if (self.settingsNavigationController) {
    // Navigate to the Google services settings if the settings dialog is
    // already opened.
    [self.settingsNavigationController
        showGoogleServicesSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController googleServicesControllerForBrowser:browser
                                                              delegate:self];

  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Remove show settings commands from MainController.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSyncSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController syncSettingsControllerForBrowser:browser
                                                            delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Remove show settings commands from MainController.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSyncPassphraseSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController syncPassphraseControllerForBrowser:browser
                                                              delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Remove show settings commands from MainController.
- (void)showSavedPasswordsSettingsFromViewController:
            (UIViewController*)baseViewController
                                    showCancelButton:(BOOL)showCancelButton {
  // Wait for dismiss to complete before trying to present a new view.
  __weak SceneController* weakSelf = self;
  [self dismissModalDialogsWithCompletion:^{
    [weakSelf
        showSavedPasswordsSettingsAfterModalDismissFromViewController:
            baseViewController
                                                     showCancelButton:
                                                         showCancelButton];
  }];
}

// Shows the Password Checkup page for `referrer`.
- (void)showPasswordCheckupPageForReferrer:
    (password_manager::PasswordCheckReferrer)referrer {
  UIViewController* baseViewController = self.currentInterface.viewController;

  [self startPasswordCheckupCoordinator:referrer];

  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showPasswordDetailsForCredential:
            (password_manager::CredentialUIEntry)credential
                              inEditMode:(BOOL)editMode {
  UIViewController* baseViewController = self.currentInterface.viewController;
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showPasswordDetailsForCredential:credential
                              inEditMode:editMode];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      passwordDetailsControllerForBrowser:browser
                                 delegate:self
                               credential:credential
                               inEditMode:editMode];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// Opens the Password Issues list displaying compromised, weak or reused
// credentials for `warningType` and `referrer`.
- (void)
    showPasswordIssuesWithWarningType:(password_manager::WarningType)warningType
                             referrer:(password_manager::PasswordCheckReferrer)
                                          referrer {
  UIViewController* baseViewController = self.currentInterface.viewController;

  [self startPasswordCheckupCoordinator:referrer];

  [self.passwordCheckupCoordinator
      showPasswordIssuesWithWarningType:warningType];

  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showAddressDetails:(autofill::AutofillProfile)address
                inEditMode:(BOOL)editMode
     offerMigrateToAccount:(BOOL)offerMigrateToAccount {
  UIViewController* baseViewController = self.currentInterface.viewController;
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
           showAddressDetails:std::move(address)
                   inEditMode:editMode
        offerMigrateToAccount:offerMigrateToAccount];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      addressDetailsControllerForBrowser:browser
                                delegate:self
                                 address:std::move(address)
                              inEditMode:editMode
                   offerMigrateToAccount:offerMigrateToAccount];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Remove show settings commands from MainController.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showProfileSettingsFromViewController:baseViewController];
    return;
  }
  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController =
      [SettingsNavigationController autofillProfileControllerForBrowser:browser
                                                               delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Remove show settings commands from MainController.
- (void)showCreditCardSettings {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showCreditCardSettings];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      autofillCreditCardControllerForBrowser:browser
                                    delegate:self];
  [self.currentInterface.viewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showCreditCardDetails:(autofill::CreditCard)creditCard
                   inEditMode:(BOOL)editMode {
  UIViewController* baseViewController = self.currentInterface.viewController;
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showCreditCardDetails:creditCard
                                                  inEditMode:editMode];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      autofillCreditCardEditControllerForBrowser:browser
                                        delegate:self
                                      creditCard:creditCard
                                      inEditMode:editMode];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showDefaultBrowserSettingsFromViewController:
            (UIViewController*)baseViewController
                                        sourceForUMA:
                                            (DefaultBrowserSettingsPageSource)
                                                source {
  if (!baseViewController) {
    baseViewController = self.currentInterface.viewController;
  }
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showDefaultBrowserSettingsFromViewController:baseViewController
                                        sourceForUMA:source];
    return;
  }
  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController =
      [SettingsNavigationController defaultBrowserControllerForBrowser:browser
                                                              delegate:self
                                                          sourceForUMA:source];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showClearBrowsingDataSettings {
  CHECK(!IsIosQuickDeleteEnabled());

  UIViewController* baseViewController = self.currentInterface.viewController;
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showClearBrowsingDataSettings];
    return;
  }
  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController = [SettingsNavigationController
      clearBrowsingDataControllerForBrowser:browser
                                   delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// Displays the Safety Check (via Settings) for `referrer`.
- (void)showAndStartSafetyCheckForReferrer:
    (password_manager::PasswordCheckReferrer)referrer {
  UIViewController* baseViewController = self.currentInterface.viewController;

  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showAndStartSafetyCheckForReferrer:referrer];
    return;
  }

  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController = [SettingsNavigationController
      safetyCheckControllerForBrowser:browser
                             delegate:self
                             referrer:referrer];

  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/330562969): Remove the deprecated function and its
// invocations.
- (void)showSafeBrowsingSettings {
  UIViewController* baseViewController = self.currentInterface.viewController;
  [self showSafeBrowsingSettingsFromViewController:baseViewController];
}

- (void)showSafeBrowsingSettingsFromViewController:
    (UIViewController*)baseViewController {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showSafeBrowsingSettings];
    return;
  }
  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController =
      [SettingsNavigationController safeBrowsingControllerForBrowser:browser
                                                            delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showSafeBrowsingSettingsFromPromoInteraction {
  DCHECK(self.settingsNavigationController);
  [self.settingsNavigationController
          showSafeBrowsingSettingsFromPromoInteraction];
}

- (void)showPasswordSearchPage {
  UIViewController* baseViewController = self.currentInterface.viewController;
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showPasswordSearchPage];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      passwordManagerSearchControllerForBrowser:browser
                                       delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showContentsSettingsFromViewController:
    (UIViewController*)baseViewController {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showContentsSettingsFromViewController:baseViewController];
    return;
  }
  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController =
      [SettingsNavigationController contentSettingsControllerForBrowser:browser
                                                               delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showNotificationsSettings {
  UIViewController* baseViewController = self.currentInterface.viewController;
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showNotificationsSettings];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      notificationsSettingsControllerForBrowser:browser
                                       delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self closeSettingsUI];
}

- (void)settingsWasDismissed {
  // Cleanup Password Checkup after its UI was dismissed.
  [self stopPasswordCheckupCoordinator];
  [self.settingsNavigationController cleanUpSettings];
  self.settingsNavigationController = nil;
}

#pragma mark - TabGridCoordinatorDelegate

- (void)tabGrid:(TabGridCoordinator*)tabGrid
    shouldActivateBrowser:(Browser*)browser
             focusOmnibox:(BOOL)focusOmnibox {
  [self beginActivatingBrowser:browser focusOmnibox:focusOmnibox];
}

- (void)tabGridDismissTransitionDidEnd:(TabGridCoordinator*)tabGrid {
  if (!self.sceneState.UIEnabled) {
    return;
  }
  [self finishActivatingBrowserDismissingTabSwitcher];
}

// Begins the process of activating the given current model, switching which BVC
// is suspended if necessary. The omnibox will be focused after the tab switcher
// dismissal is completed if `focusOmnibox` is YES.
- (void)beginActivatingBrowser:(Browser*)browser
                  focusOmnibox:(BOOL)focusOmnibox {
  DCHECK(browser == self.mainInterface.browser ||
         browser == self.incognitoInterface.browser);

  self.activatingBrowser = YES;
  ApplicationMode mode = (browser == self.mainInterface.browser)
                             ? ApplicationMode::NORMAL
                             : ApplicationMode::INCOGNITO;
  [self setCurrentInterfaceForMode:mode];

  // The call to set currentBVC above does not actually display the BVC, because
  // _activatingBrowser is YES.  So: Force the BVC transition to start.
  [self displayCurrentBVCAndFocusOmnibox:focusOmnibox];
}

// Completes the process of activating the given browser. If necessary, also
// finishes dismissing the tab switcher, removing it from the
// screen and showing the appropriate BVC.
- (void)finishActivatingBrowserDismissingTabSwitcher {
  // In real world devices, it is possible to have an empty tab model at the
  // finishing block of a BVC presentation animation. This can happen when the
  // following occur: a) There is JS that closes the last incognito tab, b) that
  // JS was paused while the user was in the tab switcher, c) the user enters
  // the tab, activating the JS while the tab is being presented. Effectively,
  // the BVC finishes the presentation animation, but there are no tabs to
  // display. The only appropriate action is to dismiss the BVC and return the
  // user to the tab switcher.
  if (self.currentInterface.browser &&
      self.currentInterface.browser->GetWebStateList() &&
      self.currentInterface.browser->GetWebStateList()->count() == 0U) {
    self.activatingBrowser = NO;
    self.modeToDisplayOnTabSwitcherDismissal = TabSwitcherDismissalMode::NONE;
    self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
    [self showTabSwitcher];
    return;
  }

  if (self.modeToDisplayOnTabSwitcherDismissal ==
      TabSwitcherDismissalMode::NORMAL) {
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  } else if (self.modeToDisplayOnTabSwitcherDismissal ==
             TabSwitcherDismissalMode::INCOGNITO) {
    [self setCurrentInterfaceForMode:ApplicationMode::INCOGNITO];
  }
  self.activatingBrowser = NO;

  self.modeToDisplayOnTabSwitcherDismissal = TabSwitcherDismissalMode::NONE;

  ProceduralBlock action = [self completionBlockForTriggeringAction:
                                     self.NTPActionAfterTabSwitcherDismissal];
  self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
  if (action) {
    action();
  }
}

#pragma mark Tab opening utility methods.

- (ProceduralBlock)completionBlockForTriggeringAction:
    (TabOpeningPostOpeningAction)action {
  __weak __typeof(self) weakSelf = self;
  switch (action) {
    case START_VOICE_SEARCH:
      return ^{
        [weakSelf startVoiceSearchInCurrentBVC];
      };
    case START_QR_CODE_SCANNER:
      return ^{
        [weakSelf startQRCodeScanner];
      };
    case START_LENS_FROM_HOME_SCREEN_WIDGET:
      return ^{
        [weakSelf startLensWithEntryPoint:LensEntrypoint::HomeScreenWidget];
      };
    case START_LENS_FROM_APP_ICON_LONG_PRESS:
      return ^{
        [weakSelf startLensWithEntryPoint:LensEntrypoint::AppIconLongPress];
      };
    case START_LENS_FROM_SPOTLIGHT:
      return ^{
        [weakSelf startLensWithEntryPoint:LensEntrypoint::Spotlight];
      };
    case START_LENS_FROM_INTENTS:
      return ^{
        [weakSelf startLensWithEntryPoint:LensEntrypoint::Intents];
      };
    case FOCUS_OMNIBOX:
      return ^{
        [weakSelf focusOmnibox];
      };
    case SHOW_DEFAULT_BROWSER_SETTINGS:
      return ^{
        [weakSelf showDefaultBrowserSettingsWithSourceForUMA:
                      DefaultBrowserSettingsPageSource::kExternalIntent];
      };
    case SEARCH_PASSWORDS:
      return ^{
        [weakSelf startPasswordSearch];
      };
    case OPEN_READING_LIST:
      return ^{
        [weakSelf openReadingList];
      };
    case OPEN_BOOKMARKS:
      return ^{
        [weakSelf openBookmarks];
      };
    case OPEN_RECENT_TABS:
      return ^{
        [weakSelf openRecentTabs];
      };
    case OPEN_TAB_GRID:
      return ^{
        [weakSelf showTabSwitcher];
      };
    case SET_CHROME_DEFAULT_BROWSER:
      return ^{
        [weakSelf showDefaultBrowserSettingsWithSourceForUMA:
                      DefaultBrowserSettingsPageSource::kExternalIntent];
      };
    case VIEW_HISTORY:
      return ^{
        [weakSelf showHistory];
      };
    case OPEN_PAYMENT_METHODS:
      return ^{
        [weakSelf openPaymentMethods];
      };
    case RUN_SAFETY_CHECK:
      return ^{
        [weakSelf showAndStartSafetyCheckForReferrer:
                      password_manager::PasswordCheckReferrer::
                          kSafetyCheckMagicStack];
      };
    case MANAGE_PASSWORDS:
      return ^{
        [weakSelf showPasswordSearchPage];
      };
    case MANAGE_SETTINGS:
      return ^{
        [weakSelf showSettingsFromViewController:weakSelf.currentInterface
                                                     .viewController];
      };
    case OPEN_LATEST_TAB:
      return ^{
        [weakSelf openLatestTab];
      };
    case OPEN_CLEAR_BROWSING_DATA_DIALOG:
      return ^{
        [weakSelf openClearBrowsingDataDialog];
      };
    case ADD_BOOKMARKS:
      return ^{
        [weakSelf addBookmarks:weakSelf.startupParameters.inputURLs];
      };
    case ADD_READING_LIST_ITEMS:
      return ^{
        [weakSelf addReadingListItems:weakSelf.startupParameters.inputURLs];
      };
    case EXTERNAL_ACTION_SHOW_BROWSER_SETTINGS:
      return ^{
        [weakSelf showDefaultBrowserSettingsWithSourceForUMA:
                      DefaultBrowserSettingsPageSource::kExternalAction];
      };
    default:
      return nil;
  }
}

// Starts a voice search on the current BVC.
- (void)startVoiceSearchInCurrentBVC {
  // If the background (non-current) BVC is playing TTS audio, call
  // -startVoiceSearch on it to stop the TTS.
  WrangledBrowser* interface = self.mainInterface == self.currentInterface
                                   ? self.incognitoInterface
                                   : self.mainInterface;
  if (interface.playingTTS) {
    [interface.bvc startVoiceSearch];
  } else {
    [self.currentInterface.bvc startVoiceSearch];
  }
}

- (void)startQRCodeScanner {
  if (!self.currentInterface.browser) {
    return;
  }
  id<QRScannerCommands> QRHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), QRScannerCommands);
  [QRHandler showQRScanner];
}

- (void)startLensWithEntryPoint:(LensEntrypoint)entryPoint {
  if (!self.currentInterface.browser) {
    return;
  }
  id<LensCommands> lensHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), LensCommands);
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:entryPoint
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromRight
      presentationCompletion:nil];
  [lensHandler openLensInputSelection:command];
}

- (void)focusOmnibox {
  if (!self.currentInterface.browser) {
    return;
  }
  id<OmniboxCommands> omniboxCommandsHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), OmniboxCommands);
  [omniboxCommandsHandler focusOmnibox];
}

- (void)showDefaultBrowserSettingsWithSourceForUMA:
    (DefaultBrowserSettingsPageSource)sourceForUMA {
  if (!self.currentInterface.browser) {
    return;
  }
  id<SettingsCommands> settingsHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), SettingsCommands);
  [settingsHandler showDefaultBrowserSettingsFromViewController:nil
                                                   sourceForUMA:sourceForUMA];
}

- (void)startPasswordSearch {
  Browser* browser = self.currentInterface.browser;
  if (!browser) {
    return;
  }
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(browser->GetProfile());
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kPasswordManagerWidgetPromoUsed);
  }

  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SettingsCommands);
  [settingsHandler showPasswordSearchPage];
}

- (void)openReadingList {
  if (!self.currentInterface.browser) {
    return;
  }
  id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                         BrowserCoordinatorCommands);
  [browserCoordinatorCommandsHandler showReadingList];
}

- (void)openBookmarks {
  if (!self.currentInterface.browser) {
    return;
  }
  id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                         BrowserCoordinatorCommands);
  [browserCoordinatorCommandsHandler showBookmarksManager];
}

- (void)openRecentTabs {
  if (!self.currentInterface.browser) {
    return;
  }
  id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                         BrowserCoordinatorCommands);
  [browserCoordinatorCommandsHandler showRecentTabs];
}

- (void)openPaymentMethods {
  if (!self.currentInterface.browser) {
    return;
  }

  id<SettingsCommands> settingsHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), SettingsCommands);
  [settingsHandler showCreditCardSettings];
}

- (void)openClearBrowsingDataDialog {
  if (!self.currentInterface.browser) {
    return;
  }

  id<SettingsCommands> settingsHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), SettingsCommands);
  [settingsHandler showClearBrowsingDataSettings];
}

- (void)openLatestTab {
  WebStateList* webStateList = self.currentInterface.browser->GetWebStateList();
  web::WebState* webState = StartSurfaceRecentTabBrowserAgent::FromBrowser(
                                self.currentInterface.browser)
                                ->most_recent_tab();
  if (!webState) {
    return;
  }
  int index = webStateList->GetIndexOfWebState(webState);
  webStateList->ActivateWebStateAt(index);
}

- (void)addBookmarks:(NSArray<NSURL*>*)URLs {
  if (!self.currentInterface.browser || [URLs count] < 1) {
    return;
  }

  id<BookmarksCommands> bookmarksCommandsHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), BookmarksCommands);

  [bookmarksCommandsHandler bulkCreateBookmarksWithURLs:URLs];
}

- (void)addReadingListItems:(NSArray<NSURL*>*)URLs {
  if (!self.currentInterface.browser || [URLs count] < 1) {
    return;
  }

  ReadingListBrowserAgent* readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(self.currentInterface.browser);

  readingListBrowserAgent->BulkAddURLsToReadingListWithViewSnackbar(URLs);
}

#pragma mark - TabOpening implementation.

- (void)dismissModalsAndMaybeOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                                 withUrlLoadParams:
                                     (const UrlLoadParams&)urlLoadParams
                                    dismissOmnibox:(BOOL)dismissOmnibox
                                        completion:(ProceduralBlock)completion {
  // Fallback to NORMAL or INCOGNITO mode if the Incognito interstitial is not
  // available.
  if (targetMode == ApplicationModeForTabOpening::UNDETERMINED) {
    PrefService* prefs = GetApplicationContext()->GetLocalState();
    BOOL canShowIncognitoInterstitial =
        prefs->GetBoolean(prefs::kIncognitoInterstitialEnabled);
    if (!canShowIncognitoInterstitial) {
      targetMode = [self isIncognitoForced]
                       ? ApplicationModeForTabOpening::INCOGNITO
                       : ApplicationModeForTabOpening::NORMAL;
    }
  }

  UrlLoadParams copyOfUrlLoadParams = urlLoadParams;

  __weak SceneController* weakSelf = self;
  void (^dismissModalsCompletion)() = ^{
    if (targetMode == ApplicationModeForTabOpening::UNDETERMINED) {
      [weakSelf showIncognitoInterstitialWithUrlLoadParams:copyOfUrlLoadParams];
      completion();
    } else {
      [weakSelf openSelectedTabInMode:targetMode
                    withUrlLoadParams:copyOfUrlLoadParams
                           completion:completion];
    }
  };

  // Wrap the post-dismiss-modals action with the incognito auth check.
  if (targetMode == ApplicationModeForTabOpening::INCOGNITO) {
    IncognitoReauthSceneAgent* reauthAgent =
        [IncognitoReauthSceneAgent agentFromScene:self.sceneState];
    if (reauthAgent.authenticationRequired) {
      void (^wrappedDismissModalCompletion)() = dismissModalsCompletion;
      dismissModalsCompletion = ^{
        [reauthAgent
            authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
              if (success) {
                wrappedDismissModalCompletion();
              } else {
                // Do not open the tab, but still call completion.
                if (completion) {
                  completion();
                }
              }
            }];
      };
    }
  }

  [self dismissModalDialogsWithCompletion:dismissModalsCompletion
                           dismissOmnibox:dismissOmnibox];
}

- (void)dismissModalsAndOpenMultipleTabsWithURLs:(const std::vector<GURL>&)URLs
                                 inIncognitoMode:(BOOL)incognitoMode
                                  dismissOmnibox:(BOOL)dismissOmnibox
                                      completion:(ProceduralBlock)completion {
  __weak SceneController* weakSelf = self;
  std::vector<GURL> copyURLs = URLs;
  [self
      dismissModalDialogsWithCompletion:^{
        [weakSelf openMultipleTabsWithURLs:copyURLs
                           inIncognitoMode:incognitoMode
                                completion:completion];
      }
                         dismissOmnibox:dismissOmnibox];
}

- (void)openTabFromLaunchWithParams:(URLOpenerParams*)params
                 startupInformation:(id<StartupInformation>)startupInformation {
  if (params) {
    [URLOpener handleLaunchOptions:params
                         tabOpener:self
             connectionInformation:self
                startupInformation:startupInformation
                       prefService:self.currentInterface.profile->GetPrefs()
                         initStage:self.sceneState.profileState.initStage];
  }
}

- (BOOL)URLIsOpenedInRegularMode:(const GURL&)URL {
  WebStateList* webStateList = self.mainInterface.browser->GetWebStateList();
  return webStateList && webStateList->GetIndexOfWebStateWithURL(URL) !=
                             WebStateList::kInvalidIndex;
}

- (BOOL)shouldOpenNTPTabOnActivationOfBrowser:(Browser*)browser {
  // Check if there are pending actions that would result in opening a new tab.
  // In that case, it is not useful to open another tab.
  for (NSUserActivity* activity in self.sceneState.connectionOptions
           .userActivities) {
    if (ActivityIsURLLoad(activity) || ActivityIsTabMove(activity)) {
      return NO;
    }
  }

  if (self.startupParameters) {
    return NO;
  }

  if (self.mainCoordinator.isTabGridActive) {
    Browser* mainBrowser = self.mainInterface.browser;
    Browser* otrBrowser = self.incognitoInterface.browser;
    // Only attempt to dismiss the tab switcher and open a new tab if:
    // - there are no tabs open in either tab model, and
    // - the tab switcher controller is not directly or indirectly presenting
    // another view controller.
    if (!(mainBrowser->GetWebStateList()->empty()) ||
        !(otrBrowser->GetWebStateList()->empty())) {
      return NO;
    }

    // If the tabSwitcher is contained, check if the parent container is
    // presenting another view controller.
    if ([self.mainCoordinator.baseViewController
                .parentViewController presentedViewController]) {
      return NO;
    }

    // Check if the tabSwitcher is directly presenting another view controller.
    if (self.mainCoordinator.baseViewController.presentedViewController) {
      return NO;
    }

    return YES;
  }

  return browser->GetWebStateList()->empty();
}

#pragma mark - SceneURLLoadingServiceDelegate

// Note that the current tab of `browserCoordinator`'s BVC will normally be
// reloaded by this method. If a new tab is about to be added, call
// expectNewForegroundTab on the BVC first to avoid extra work and possible page
// load side-effects for the tab being replaced.
- (void)setCurrentInterfaceForMode:(ApplicationMode)mode {
  DCHECK(self.browserViewWrangler);
  BOOL incognito = mode == ApplicationMode::INCOGNITO;
  WrangledBrowser* currentInterface = self.currentInterface;
  WrangledBrowser* newInterface =
      incognito ? self.incognitoInterface : self.mainInterface;
  if (currentInterface && currentInterface == newInterface) {
    return;
  }

  // Update the snapshot before switching another application mode.  This
  // ensures that the snapshot is correct when links are opened in a different
  // application mode.
  [self updateActiveWebStateSnapshot];

  self.browserViewWrangler.currentInterface = newInterface;

  if (!self.activatingBrowser) {
    [self displayCurrentBVCAndFocusOmnibox:NO];
  }

  // Tell the BVC that was made current that it can use the web.
  [self activateBVCAndMakeCurrentBVCPrimary];
}

- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  // Disconnected scenes should no-op, since browser objects may not exist.
  // See crbug.com/371847600.
  if (self.sceneState.activationLevel == SceneActivationLevelDisconnected) {
    return;
  }

  // Immediately hide modals from the provider (alert views, action sheets,
  // popovers). They will be ultimately dismissed by their owners, but at least,
  // they are not visible.
  ios::provider::HideModalViewStack();

  // Exit fullscreen mode for web page when we re-enter app through external
  // intents.
  web::WebState* webState =
      self.mainInterface.browser->GetWebStateList()->GetActiveWebState();
  if (webState && webState->IsWebPageInFullscreenMode()) {
    webState->CloseMediaPresentations();
  }

  // ChromeIdentityService is responsible for the dialogs displayed by the
  // services it wraps.
  GetApplicationContext()->GetSystemIdentityManager()->DismissDialogs();

  // MailtoHandlerService is responsible for the dialogs displayed by the
  // services it wraps.
  MailtoHandlerServiceFactory::GetForProfile(self.currentInterface.profile)
      ->DismissAllMailtoHandlerInterfaces();

  // Then, depending on what the SSO view controller is presented on, dismiss
  // it.
  ProceduralBlock completionWithBVC = ^{
    DCHECK(self.currentInterface.viewController);
    DCHECK(!self.mainCoordinator.isTabGridActive);
    DCHECK(!self.signinCoordinator)
        << "self.signinCoordinator: "
        << base::SysNSStringToUTF8([self.signinCoordinator description]);
    // This will dismiss the SSO view controller.
    [self.currentInterface clearPresentedStateWithCompletion:completion
                                              dismissOmnibox:dismissOmnibox];
  };
  ProceduralBlock completionWithoutBVC = ^{
    // `self.currentInterface.bvc` may exist but tab switcher should be
    // active.
    DCHECK(self.mainCoordinator.isTabGridActive);
    DCHECK(!self.signinCoordinator)
        << "self.signinCoordinator: "
        << base::SysNSStringToUTF8([self.signinCoordinator description]);
    // History coordinator can be started on top of the tab grid.
    // This is not true of the other tab switchers.
    DCHECK(self.mainCoordinator);
    [self.mainCoordinator stopChildCoordinatorsWithCompletion:completion];
  };

  // Select a completion based on whether the BVC is shown.
  ProceduralBlock chosenCompletion = self.mainCoordinator.isTabGridActive
                                         ? completionWithoutBVC
                                         : completionWithBVC;

  [self closePresentedViews:NO completion:chosenCompletion];

  // Verify that no modal views are left presented.
  ios::provider::LogIfModalViewsArePresented();
}

- (void)openMultipleTabsWithURLs:(const std::vector<GURL>&)URLs
                 inIncognitoMode:(BOOL)openInIncognito
                      completion:(ProceduralBlock)completion {
  [self recursiveOpenURLs:URLs
          inIncognitoMode:openInIncognito
             currentIndex:0
               totalCount:URLs.size()
               completion:completion];
}

// Call `dismissModalsAndMaybeOpenSelectedTabInMode` recursively to open the
// list of URLs contained in `URLs`. Achieved through chaining
// `dismissModalsAndMaybeOpenSelectedTabInMode` in its completion handler.
- (void)recursiveOpenURLs:(const std::vector<GURL>&)URLs
          inIncognitoMode:(BOOL)incognitoMode
             currentIndex:(size_t)currentIndex
               totalCount:(size_t)totalCount
               completion:(ProceduralBlock)completion {
  if (currentIndex >= totalCount) {
    if (completion) {
      completion();
    }
    return;
  }

  GURL webpageGURL = URLs.at(currentIndex);

  __weak SceneController* weakSelf = self;

  if (!webpageGURL.is_valid()) {
    [self recursiveOpenURLs:URLs
            inIncognitoMode:incognitoMode
               currentIndex:(currentIndex + 1)
                 totalCount:totalCount
                 completion:completion];
    return;
  }

  UrlLoadParams param = UrlLoadParams::InNewTab(webpageGURL, webpageGURL);
  std::vector<GURL> copyURLs = URLs;

  ApplicationModeForTabOpening mode =
      incognitoMode ? ApplicationModeForTabOpening::INCOGNITO
                    : ApplicationModeForTabOpening::NORMAL;
  [self
      dismissModalsAndMaybeOpenSelectedTabInMode:mode
                               withUrlLoadParams:param
                                  dismissOmnibox:YES
                                      completion:^{
                                        [weakSelf
                                            recursiveOpenURLs:copyURLs
                                              inIncognitoMode:incognitoMode
                                                 currentIndex:(currentIndex + 1)
                                                   totalCount:totalCount
                                                   completion:completion];
                                      }];
}

// Opens a tab in the target BVC, and switches to it in a way that's appropriate
// to the current UI, based on the `dismissModals` flag:
// - If a modal dialog is showing and `dismissModals` is NO, the selected tab of
// the main tab model will change in the background, but the view won't change.
// - Otherwise, any modal view will be dismissed, the tab switcher will animate
// out if it is showing, the target BVC will become active, and the new tab will
// be shown.
// If the current tab in `targetMode` is a NTP, it can be reused to open URL.
// `completion` is executed after the tab is opened. After Tab is open the
// virtual URL is set to the pending navigation item.
- (void)openSelectedTabInMode:(ApplicationModeForTabOpening)tabOpeningTargetMode
            withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                   completion:(ProceduralBlock)completion {
  DCHECK(tabOpeningTargetMode != ApplicationModeForTabOpening::UNDETERMINED);
  // Update the snapshot before opening a new tab. This ensures that the
  // snapshot is correct when tabs are openned via the dispatcher.
  [self updateActiveWebStateSnapshot];

  ApplicationMode targetMode;

  if (tabOpeningTargetMode == ApplicationModeForTabOpening::CURRENT) {
    targetMode = self.currentInterface.incognito ? ApplicationMode::INCOGNITO
                                                 : ApplicationMode::NORMAL;
  } else if (tabOpeningTargetMode == ApplicationModeForTabOpening::NORMAL) {
    targetMode = ApplicationMode::NORMAL;
  } else {
    targetMode = ApplicationMode::INCOGNITO;
  }

  WrangledBrowser* targetInterface = targetMode == ApplicationMode::NORMAL
                                         ? self.mainInterface
                                         : self.incognitoInterface;
  ProceduralBlock startupCompletion =
      [self completionBlockForTriggeringAction:[self.startupParameters
                                                       postOpeningAction]];

  ProceduralBlock tabOpenedCompletion = nil;
  if (startupCompletion && completion) {
    tabOpenedCompletion = ^{
      // Order is important here. `completion` may do cleaning tasks that will
      // invalidate `startupCompletion`.
      startupCompletion();
      completion();
    };
  } else if (startupCompletion) {
    tabOpenedCompletion = startupCompletion;
  } else {
    tabOpenedCompletion = completion;
  }

  if (self.mainCoordinator.isTabGridActive) {
    // If the tab switcher is already being dismissed, simply add the tab and
    // note that when the tab switcher finishes dismissing, the current BVC
    // should be switched to be the main BVC if necessary.
    if (self.activatingBrowser) {
      self.modeToDisplayOnTabSwitcherDismissal =
          targetMode == ApplicationMode::NORMAL
              ? TabSwitcherDismissalMode::NORMAL
              : TabSwitcherDismissalMode::INCOGNITO;
      [targetInterface.bvc appendTabAddedCompletion:tabOpenedCompletion];
      UrlLoadParams savedParams = urlLoadParams;
      savedParams.in_incognito = targetMode == ApplicationMode::INCOGNITO;
      UrlLoadingBrowserAgent::FromBrowser(targetInterface.browser)
          ->Load(savedParams);
    } else {
      // Voice search, QRScanner, Lens, and the omnibox are presented by the
      // BVC. They must be started after the BVC view is added in the
      // hierarchy.
      self.NTPActionAfterTabSwitcherDismissal =
          [self.startupParameters postOpeningAction];
      [self setStartupParameters:nil];

      UrlLoadParams paramsToLoad = urlLoadParams;
      // If the url to load is empty (such as with Lens) open a new tab page.
      if (urlLoadParams.web_params.url.is_empty()) {
        paramsToLoad = UrlLoadParams(urlLoadParams);
        paramsToLoad.web_params.url = GURL(kChromeUINewTabURL);
      }

      [self addANewTabAndPresentBrowser:targetInterface.browser
                      withURLLoadParams:paramsToLoad];

      // In this particular usage, there should be no postOpeningAction,
      // as triggering voice search while there are multiple windows opened is
      // probably a bad idea both technically and as a user experience. It
      // should be the caller duty to not set a completion if they don't need
      // it.
      if (completion) {
        completion();
      }
    }
  } else {
    if (!self.currentInterface.viewController.presentedViewController) {
      PagePlaceholderBrowserAgent* pagePlaceholderBrowserAgent =
          PagePlaceholderBrowserAgent::FromBrowser(targetInterface.browser);
      pagePlaceholderBrowserAgent->ExpectNewForegroundTab();
    }
    [self setCurrentInterfaceForMode:targetMode];
    [self openOrReuseTabInMode:targetMode
             withUrlLoadParams:urlLoadParams
           tabOpenedCompletion:tabOpenedCompletion];
  }
}

- (void)expectNewForegroundTabForMode:(ApplicationMode)targetMode {
  WrangledBrowser* interface = targetMode == ApplicationMode::INCOGNITO
                                   ? self.incognitoInterface
                                   : self.mainInterface;
  DCHECK(interface);
  PagePlaceholderBrowserAgent* pagePlaceholderBrowserAgent =
      PagePlaceholderBrowserAgent::FromBrowser(interface.browser);
  pagePlaceholderBrowserAgent->ExpectNewForegroundTab();
}

- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox
                    inheritOpener:(BOOL)inheritOpener {
  [self.currentInterface.bvc openNewTabFromOriginPoint:originPoint
                                          focusOmnibox:focusOmnibox
                                         inheritOpener:inheritOpener];
}

- (Browser*)currentBrowserForURLLoading {
  return self.currentInterface.browser;
}

// Asks the respective Snapshot helper to update the snapshot for the active
// WebState.
- (void)updateActiveWebStateSnapshot {
  // Durinhg startup, there may be no current interface. Do nothing in that
  // case.
  if (!self.currentInterface) {
    return;
  }

  WebStateList* webStateList = self.currentInterface.browser->GetWebStateList();
  web::WebState* webState = webStateList->GetActiveWebState();
  if (webState) {
    SnapshotTabHelper::FromWebState(webState)->UpdateSnapshotWithCallback(nil);
  }
}

// Checks the target BVC's current tab's URL. If `urlLoadParams` has an empty
// URL, no new tab will be opened and `tabOpenedCompletion` will be run. If this
// URL is chrome://newtab, loads `urlLoadParams` in this tab. Otherwise, open
// `urlLoadParams` in a new tab in the target BVC. `tabOpenedCompletion` will be
// called on the new tab (if not nil).
- (void)openOrReuseTabInMode:(ApplicationMode)targetMode
           withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
         tabOpenedCompletion:(ProceduralBlock)tabOpenedCompletion {
  WrangledBrowser* targetInterface = targetMode == ApplicationMode::NORMAL
                                         ? self.mainInterface
                                         : self.incognitoInterface;
  // If the url to load is empty, create a new tab if no tabs are open and run
  // the completion.
  if (urlLoadParams.web_params.url.is_empty()) {
    if (tabOpenedCompletion) {
      tabOpenedCompletion();
    }
    return;
  }

  BrowserViewController* targetBVC = targetInterface.bvc;
  web::WebState* currentWebState =
      targetInterface.browser->GetWebStateList()->GetActiveWebState();

  BOOL alwaysInsertNewTab =
      base::FeatureList::IsEnabled(kForceNewTabForIntentSearch) &&
      (self.startupParameters.postOpeningAction == FOCUS_OMNIBOX);

  // Don't call loadWithParams for chrome://newtab when it's already loaded.
  // Note that it's safe to use -GetVisibleURL here, as it doesn't matter if the
  // NTP hasn't finished loading.
  if (!alwaysInsertNewTab && currentWebState &&
      IsUrlNtp(currentWebState->GetVisibleURL()) &&
      IsUrlNtp(urlLoadParams.web_params.url)) {
    if (tabOpenedCompletion) {
      tabOpenedCompletion();
    }
    return;
  }

  if (urlLoadParams.disposition == WindowOpenDisposition::SWITCH_TO_TAB) {
    // Check if it's already the displayed tab and no switch is necessary
    if (currentWebState &&
        currentWebState->GetVisibleURL() == urlLoadParams.web_params.url) {
      if (tabOpenedCompletion) {
        tabOpenedCompletion();
      }
      return;
    }

    // Check if this tab exists in this web state list.
    // If not, fall back to opening a new tab instead.
    if (targetInterface.browser->GetWebStateList()->GetIndexOfWebStateWithURL(
            urlLoadParams.web_params.url) != WebStateList::kInvalidIndex) {
      UrlLoadingBrowserAgent::FromBrowser(targetInterface.browser)
          ->Load(urlLoadParams);
      if (tabOpenedCompletion) {
        tabOpenedCompletion();
      }
      return;
    }
  }

  // If the current tab isn't an NTP, open a new tab.  Be sure to use
  // -GetLastCommittedURL incase the NTP is still loading.
  if (alwaysInsertNewTab ||
      !(currentWebState && IsUrlNtp(currentWebState->GetVisibleURL()))) {
    [targetBVC appendTabAddedCompletion:tabOpenedCompletion];
    UrlLoadParams newTabParams = urlLoadParams;
    newTabParams.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    newTabParams.in_incognito = targetMode == ApplicationMode::INCOGNITO;
    UrlLoadingBrowserAgent::FromBrowser(targetInterface.browser)
        ->Load(newTabParams);
    return;
  }

  // Otherwise, load `urlLoadParams` in the current tab.
  UrlLoadParams sameTabParams = urlLoadParams;
  sameTabParams.disposition = WindowOpenDisposition::CURRENT_TAB;
  UrlLoadingBrowserAgent::FromBrowser(targetInterface.browser)
      ->Load(sameTabParams);
  if (tabOpenedCompletion) {
    tabOpenedCompletion();
  }
}

// Displays current (incognito/normal) BVC and optionally focuses the omnibox.
- (void)displayCurrentBVCAndFocusOmnibox:(BOOL)focusOmnibox {
  ProceduralBlock completion = nil;
  if (focusOmnibox) {
    id<OmniboxCommands> omniboxHandler = HandlerForProtocol(
        self.currentInterface.browser->GetCommandDispatcher(), OmniboxCommands);
    completion = ^{
      [omniboxHandler focusOmnibox];
    };
  }
  [self.mainCoordinator
      showTabViewController:self.currentInterface.viewController
                  incognito:self.currentInterface.incognito
                 completion:completion];
  [HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                      ApplicationCommands)
      setIncognitoContentVisible:self.currentInterface.incognito];
}

#pragma mark - Sign In UI presentation

// Show trusted vault dialog.
// `intent` Dialog to present.
// `trigger` UI elements where the trusted vault reauth has been triggered.
- (void)
    showTrustedVaultDialogFromViewController:(UIViewController*)viewController
                                      intent:
                                          (SigninTrustedVaultDialogIntent)intent
                            securityDomainID:(trusted_vault::SecurityDomainId)
                                                 securityDomainID
                                     trigger:
                                         (syncer::
                                              TrustedVaultUserActionTriggerForUMA)
                                             trigger
                                 accessPoint:
                                     (signin_metrics::AccessPoint)accessPoint {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  Browser* mainBrowser = self.mainInterface.browser;
  self.signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordinatorWithBaseViewController:
          viewController
                                                            browser:mainBrowser
                                                             intent:intent
                                                   securityDomainID:
                                                       securityDomainID
                                                            trigger:trigger
                                                        accessPoint:
                                                            accessPoint];
  [self startSigninCoordinatorWithCompletion:nil];
}

// Close Settings, or Signin or the 3rd-party intents Incognito interstitial.
- (void)closePresentedViews:(BOOL)animated
                 completion:(ProceduralBlock)completion {
  // If the Incognito interstitial is active, stop it.
  [self.incognitoInterstitialCoordinator stop];
  self.incognitoInterstitialCoordinator = nil;

  // If History is active, stop it.
  [self.historyCoordinator stop];
  self.historyCoordinator = nil;

  __weak __typeof(self) weakSelf = self;
  BOOL resetSigninState = self.signinCoordinator != nil;
  completion = ^{
    __typeof(self) strongSelf = weakSelf;
    // Cleanup settings resources after dismissal.
    [strongSelf settingsWasDismissed];
    if (completion) {
      completion();
    }
    if (resetSigninState) {
      strongSelf.sceneState.signinInProgress = NO;
    }
    strongSelf.dismissingSigninPromptFromExternalTrigger = NO;
  };

  if (self.settingsNavigationController && !self.dismissingSettings) {
    self.dismissingSettings = YES;
    // Store a reference to the presentingViewController in case the user
    // is dismissing the Signin screen and then dismisses Settings before
    // the Signin screen is done animating, which will delay the execution of
    // the `dismissSettings` block stopping the code from accessing
    // the `presentingViewController` property.
    __weak UIViewController* weakPresentingViewController =
        [self.settingsNavigationController presentingViewController];
    ProceduralBlock dismissSettings = ^() {
      UIViewController* strongPresentingViewController =
          weakPresentingViewController;
      if (strongPresentingViewController) {
        [strongPresentingViewController
            dismissViewControllerAnimated:animated
                               completion:completion];
      } else {
        // The view is already dismissed. Completion should still be called.
        completion();
      }
      weakSelf.dismissingSettings = NO;
    };
    // `self.signinCoordinator` can be presented on top of the settings, to
    // present the Trusted Vault reauthentication `self.signinCoordinator` has
    // to be closed first.
    if (self.signinCoordinator) {
      // If signinCoordinator is already dismissing, completion execution will
      // happen when it is done animating.
      [self interruptSigninCoordinatorAnimated:animated
                                    completion:dismissSettings];
    } else {
      dismissSettings();
    }
  } else if (self.signinCoordinator) {
    // `self.signinCoordinator` can be presented without settings, from the
    // bookmarks or the recent tabs view.
    [self interruptSigninCoordinatorAnimated:animated completion:completion];
  } else {
    completion();
  }
}

- (UIViewController*)topPresentedViewController {
  // TODO(crbug.com/40534720): Implement TopPresentedViewControllerFrom()
  // privately.
  return top_view_controller::TopPresentedViewControllerFrom(
      self.mainCoordinator.baseViewController);
}

// Interrupts the sign-in coordinator actions and dismisses its views either
// with or without animation.
- (void)interruptSigninCoordinatorAnimated:(BOOL)animated
                                completion:(ProceduralBlock)completion {
  DCHECK(self.signinCoordinator);
  SigninCoordinatorInterrupt action =
      animated ? SigninCoordinatorInterrupt::DismissWithAnimation
               : SigninCoordinatorInterrupt::DismissWithoutAnimation;

  self.dismissingSigninPromptFromExternalTrigger = YES;
  [self.signinCoordinator interruptWithAction:action completion:completion];
}

// Starts the sign-in coordinator with a default cleanup completion.
- (void)startSigninCoordinatorWithCompletion:
    (ShowSigninCommandCompletionCallback)completion {
  DCHECK(self.signinCoordinator);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(
          self.sceneState.browserProviderInterface.mainBrowserProvider.browser
              ->GetProfile());
  AuthenticationService::ServiceStatus statusService =
      authenticationService->GetServiceStatus();
  switch (statusService) {
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy: {
      if (completion) {
        completion(SigninCoordinatorResultDisabled, nil);
      }
      [self.signinCoordinator stop];
      id<PolicyChangeCommands> handler = HandlerForProtocol(
          self.signinCoordinator.browser->GetCommandDispatcher(),
          PolicyChangeCommands);
      [handler showForceSignedOutPrompt];
      self.signinCoordinator = nil;
      return;
    }
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed: {
      break;
    }
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
    case AuthenticationService::ServiceStatus::SigninDisabledByUser: {
      DUMP_WILL_BE_NOTREACHED()
          << "Status service: " << static_cast<int>(statusService);
      break;
    }
  }

  DCHECK(self.signinCoordinator);
  self.sceneState.signinInProgress = YES;

  __block std::unique_ptr<ScopedUIBlocker> uiBlocker =
      std::make_unique<ScopedUIBlocker>(self.sceneState);
  __weak SceneController* weakSelf = self;
  self.signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
        if (!weakSelf) {
          return;
        }
        __typeof(self) strongSelf = weakSelf;
        [strongSelf.signinCoordinator stop];
        strongSelf.signinCoordinator = nil;
        uiBlocker.reset();

        if (completion) {
          completion(result, info);
        }

        if (!weakSelf.dismissingSigninPromptFromExternalTrigger) {
          // If the coordinator isn't stopped by an external trigger, sign-in
          // is done. Otherwise, there might be extra steps to be done before
          // considering sign-in as done. This is up to the handler that sets
          // `self.dismissingSigninPromptFromExternalTrigger` to YES to set
          // back `signinInProgress` to NO.
          weakSelf.sceneState.signinInProgress = NO;
        }

        switch (info.signinCompletionAction) {
          case SigninCompletionActionNone:
            break;
          case SigninCompletionActionShowManagedLearnMore:
            id<ApplicationCommands> dispatcher = HandlerForProtocol(
                strongSelf.mainInterface.browser->GetCommandDispatcher(),
                ApplicationCommands);
            OpenNewTabCommand* command = [OpenNewTabCommand
                commandWithURLFromChrome:GURL(kChromeUIManagementURL)];
            [dispatcher closeSettingsUIAndOpenURL:command];
            break;
        }

        if (IsSigninForcedByPolicy()) {
          // Handle intents after sign-in is done when the forced sign-in policy
          // is enabled.
          [strongSelf handleExternalIntents];
        }
      };

  [self.signinCoordinator start];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      // Do nothing during batch operation.
      if (webStateList->IsBatchInProgress()) {
        break;
      }

      if (webStateList->empty()) {
        [self onLastWebStateClosedForWebStateList:webStateList];
      }
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace:
      // Do nothing when a WebState is replaced.
      break;
    case WebStateListChange::Type::kInsert:
      // Do nothing when a WebState is inserted.
      break;
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  _tabCountBeforeBatchOperation.insert(
      std::make_pair(webStateList, webStateList->count()));
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  auto iter = _tabCountBeforeBatchOperation.find(webStateList);
  DCHECK(iter != _tabCountBeforeBatchOperation.end());

  // Triggers the switcher view if the list is empty and at least one tab
  // was closed (i.e. it was not empty before the batch operation).
  if (webStateList->empty() && iter->second != 0) {
    [self onLastWebStateClosedForWebStateList:webStateList];
  }

  _tabCountBeforeBatchOperation.erase(iter);
}

#pragma mark - Private methods

// Triggers the switcher view when the last WebState is closed on a device
// that uses the switcher.
- (void)onLastWebStateClosedForWebStateList:(WebStateList*)webStateList {
  DCHECK(webStateList->empty());
  if (webStateList == self.incognitoInterface.browser->GetWebStateList()) {
    [self lastIncognitoTabClosed];
  } else if (webStateList == self.mainInterface.browser->GetWebStateList()) {
    [self lastRegularTabClosed];
  }
}

// Open a non-incognito tab, if one exists. If one doesn't exist, open a new
// one. If incognito is forced, an incognito tab will be opened.
- (void)openNonIncognitoTab:(ProceduralBlock)completion {
  if (self.mainInterface.browser->GetWebStateList()->GetActiveWebState()) {
    // Reuse an existing tab, if one exists.
    ApplicationMode mode = [self isIncognitoForced] ? ApplicationMode::INCOGNITO
                                                    : ApplicationMode::NORMAL;
    [self setCurrentInterfaceForMode:mode];
    if (self.mainCoordinator.isTabGridActive) {
      [self.mainCoordinator
          showTabViewController:self.currentInterface.viewController
                      incognito:self.currentInterface.incognito
                     completion:completion];
      [self setIncognitoContentVisible:self.currentInterface.incognito];
    } else {
      if (completion) {
        completion();
      }
    }
  } else {
    // Open a new NTP.
    UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
    params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
    ApplicationModeForTabOpening mode =
        [self isIncognitoForced] ? ApplicationModeForTabOpening::INCOGNITO
                                 : ApplicationModeForTabOpening::NORMAL;
    [self dismissModalsAndMaybeOpenSelectedTabInMode:mode
                                   withUrlLoadParams:params
                                      dismissOmnibox:YES
                                          completion:completion];
  }
}

// Ensures that a non-incognito NTP tab is open. If incognito is forced, then
// it will ensure an incognito NTP tab is open.
- (void)ensureNTP {
  // If the tab does not exist, open a new tab.
  UrlLoadParams params = UrlLoadParams::InCurrentTab(GURL(kChromeUINewTabURL));
  ApplicationMode mode = self.currentInterface.incognito
                             ? ApplicationMode::INCOGNITO
                             : ApplicationMode::NORMAL;
  [self openOrReuseTabInMode:mode
           withUrlLoadParams:params
         tabOpenedCompletion:nil];
}

// Stops and deletes `passwordCheckupCoordinator`.
- (void)stopPasswordCheckupCoordinator {
  [self.passwordCheckupCoordinator stop];

  self.passwordCheckupCoordinator.delegate = nil;
  self.passwordCheckupCoordinator = nil;
}

#pragma mark - IncognitoInterstitialCoordinatorDelegate

- (void)shouldStopIncognitoInterstitial:
    (IncognitoInterstitialCoordinator*)incognitoInterstitial {
  DCHECK(incognitoInterstitial == self.incognitoInterstitialCoordinator);
  [self closePresentedViews:YES completion:nil];
}

#pragma mark - PasswordCheckupCoordinatorDelegate

- (void)passwordCheckupCoordinatorDidRemove:
    (PasswordCheckupCoordinator*)coordinator {
  DCHECK_EQ(self.passwordCheckupCoordinator, coordinator);

  [self stopPasswordCheckupCoordinator];
}

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  [self closeSettingsUI];
}

#pragma mark - Helpers for web state list events

// Called when the last incognito tab was closed.
- (void)lastIncognitoTabClosed {
  // If no other window has incognito tab, then destroy and rebuild the
  // Profile. Otherwise, just do the state transition animation.
  if ([self shouldDestroyAndRebuildIncognitoProfile]) {
    // Incognito profile cannot be deleted before all the requests are
    // deleted. Queue empty task on IO thread and destroy the Profile
    // when the task has executed, again verifying that no incognito tabs are
    // present. When an incognito tab is moved between browsers, there is
    // a point where the tab isn't attached to any web state list. However, when
    // this queued cleanup step executes, the moved tab will be attached, so
    // the cleanup shouldn't proceed.

    auto cleanup = ^{
      if ([self shouldDestroyAndRebuildIncognitoProfile]) {
        [self destroyAndRebuildIncognitoProfile];
      }
    };

    web::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), base::BindRepeating(cleanup));
  }

  // a) The first condition can happen when the last incognito tab is closed
  // from the tab switcher.
  // b) The second condition can happen if some other code (like JS) triggers
  // closure of tabs from the otr tab model when it's not current.
  // Nothing to do here. The next user action (like clicking on an existing
  // regular tab or creating a new incognito tab from the settings menu) will
  // take care of the logic to mode switch.
  if (self.mainCoordinator.isTabGridActive ||
      !self.currentInterface.incognito) {
    return;
  }
  [self showTabSwitcher];
}

// Called when the last regular tab was closed.
- (void)lastRegularTabClosed {
  // a) The first condition can happen when the last regular tab is closed from
  // the tab switcher.
  // b) The second condition can happen if some other code (like JS) triggers
  // closure of tabs from the main tab model when the main tab model is not
  // current.
  // Nothing to do here.
  if (self.mainCoordinator.isTabGridActive || self.currentInterface.incognito) {
    return;
  }

  [self showTabSwitcher];
}

// Clears incognito data that is specific to iOS and won't be cleared by
// deleting the profile.
- (void)clearIOSSpecificIncognitoData {
  DCHECK(self.sceneState.browserProviderInterface.mainBrowserProvider.browser
             ->GetProfile()
             ->HasOffTheRecordProfile());
  ProfileIOS* otrProfile =
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser
          ->GetProfile()
          ->GetOffTheRecordProfile();

  __weak SceneController* weakSelf = self;
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForProfile(otrProfile);
  browsingDataRemover->Remove(browsing_data::TimePeriod::ALL_TIME,
                              BrowsingDataRemoveMask::REMOVE_ALL,
                              base::BindOnce(^{
                                [weakSelf activateBVCAndMakeCurrentBVCPrimary];
                              }));
}

- (void)activateBVCAndMakeCurrentBVCPrimary {
  // If there are pending removal operations, the activation will be deferred
  // until the callback is received.
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForProfileIfExists(
          self.currentInterface.profile);
  if (browsingDataRemover && browsingDataRemover->IsRemoving()) {
    return;
  }

  WebUsageEnablerBrowserAgent::FromBrowser(self.mainInterface.browser)
      ->SetWebUsageEnabled(true);
  WebUsageEnablerBrowserAgent::FromBrowser(self.incognitoInterface.browser)
      ->SetWebUsageEnabled(true);

  if (self.currentInterface) {
    TabUsageRecorderBrowserAgent* tabUsageRecorder =
        TabUsageRecorderBrowserAgent::FromBrowser(
            self.currentInterface.browser);
    if (tabUsageRecorder) {
      tabUsageRecorder->RecordPrimaryBrowserChange(true);
    }
  }
}

// Shows the tab switcher UI.
- (void)showTabSwitcher {
  DCHECK(self.mainCoordinator);
  [self.mainCoordinator setActiveMode:TabGridMode::kNormal];
  TabGridPage page =
      (self.currentInterface.browser == self.incognitoInterface.browser)
          ? TabGridPageIncognitoTabs
          : TabGridPageRegularTabs;

  [self.mainCoordinator showTabGridPage:page];
}

- (void)openURLContexts:(NSSet<UIOpenURLContext*>*)URLContexts {
  if (self.sceneState.profileState.initStage <= ProfileInitStage::kUIReady ||
      !self.currentInterface.profile) {
    // Don't handle the intent if the browser UI objects aren't yet initialized.
    // This is the case when the app is in safe mode or may be the case when the
    // app is going through an odd sequence of lifecyle events (shouldn't happen
    // but happens somehow), see crbug.com/1211006 for more details.
    return;
  }

  NSMutableSet<URLOpenerParams*>* URLsToOpen = [[NSMutableSet alloc] init];
  for (UIOpenURLContext* context : URLContexts) {
    URLOpenerParams* options =
        [[URLOpenerParams alloc] initWithUIOpenURLContext:context];
    NSSet* URLContextSet = [NSSet setWithObject:context];
    if (!GetApplicationContext()
             ->GetSystemIdentityManager()
             ->HandleSessionOpenURLContexts(self.sceneState.scene,
                                            URLContextSet)) {
      [URLsToOpen addObject:options];
    }
  }
  // When opening with URLs for GetChromeIdentityService, it is expected that a
  // single URL is passed.
  DCHECK(URLsToOpen.count == URLContexts.count || URLContexts.count == 1);
  BOOL active = [self canHandleIntents];

  for (URLOpenerParams* options : URLsToOpen) {
    [URLOpener openURL:options
            applicationActive:active
                    tabOpener:self
        connectionInformation:self
           startupInformation:self.sceneState.appState.startupInformation
                  prefService:self.currentInterface.profile->GetPrefs()
                    initStage:self.sceneState.profileState.initStage];
  }
}

- (WrangledBrowser*)extractInterfaceBaseOnMode:
    (ApplicationModeForTabOpening)targetMode {
  DCHECK(targetMode != ApplicationModeForTabOpening::UNDETERMINED);
  ApplicationMode applicationMode;

  if (targetMode == ApplicationModeForTabOpening::CURRENT) {
    applicationMode = self.currentInterface.incognito
                          ? ApplicationMode::INCOGNITO
                          : ApplicationMode::NORMAL;
  } else if (targetMode == ApplicationModeForTabOpening::NORMAL) {
    applicationMode = ApplicationMode::NORMAL;
  } else {
    applicationMode = ApplicationMode::INCOGNITO;
  }

  WrangledBrowser* targetInterface = applicationMode == ApplicationMode::NORMAL
                                         ? self.mainInterface
                                         : self.incognitoInterface;

  return targetInterface;
}

#pragma mark - TabGrid helpers

// Adds a new tab to the `browser` based on `urlLoadParams` and then presents
// it.
- (void)addANewTabAndPresentBrowser:(Browser*)browser
                  withURLLoadParams:(const UrlLoadParams&)urlLoadParams {
  TabInsertion::Params tabInsertionParams;
  tabInsertionParams.should_skip_new_tab_animation =
      urlLoadParams.from_external;
  TabInsertionBrowserAgent::FromBrowser(browser)->InsertWebState(
      urlLoadParams.web_params, tabInsertionParams);
  [self beginActivatingBrowser:browser focusOmnibox:NO];
}

#pragma mark - Handling of destroying the incognito profile

// The incognito Profile should be closed when the last incognito tab is
// closed (i.e. if there are other incognito tabs open in another Scene, the
// Profile must not be destroyed).
- (BOOL)shouldDestroyAndRebuildIncognitoProfile {
  ProfileIOS* profile = self.sceneState.browserProviderInterface
                            .mainBrowserProvider.browser->GetProfile();
  if (!profile->HasOffTheRecordProfile()) {
    return NO;
  }

  ProfileIOS* otrProfile = profile->GetOffTheRecordProfile();
  DCHECK(otrProfile);

  BrowserList* browserList = BrowserListFactory::GetForProfile(otrProfile);
  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kIncognito)) {
    WebStateList* webStateList = browser->GetWebStateList();
    if (!webStateList->empty()) {
      return NO;
    }
  }

  return YES;
}

// Destroys and rebuilds the incognito Profile. This will inform all the
// other SceneController to destroy state tied to the Profile and to
// recreate it.
- (void)destroyAndRebuildIncognitoProfile {
  // This seems the best place to mark the start of destroying the incognito
  // profile.
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(
      /*in_progress=*/true);

  [self clearIOSSpecificIncognitoData];

  ProfileIOS* profile = self.sceneState.browserProviderInterface
                            .mainBrowserProvider.browser->GetProfile();
  DCHECK(profile->HasOffTheRecordProfile());
  ProfileIOS* otrProfile = profile->GetOffTheRecordProfile();

  NSMutableArray<SceneController*>* sceneControllers =
      [[NSMutableArray alloc] init];
  for (SceneState* sceneState in self.sceneState.profileState.connectedScenes) {
    SceneController* sceneController = sceneState.controller;
    // In some circumstances, the scene state may still exist while the
    // corresponding scene controller has been deallocated.
    // (see crbug.com/1142782).
    if (sceneController) {
      [sceneControllers addObject:sceneController];
    }
  }

  for (SceneController* sceneController in sceneControllers) {
    [sceneController willDestroyIncognitoProfile];
  }

  // Record off-the-record metrics before detroying the Profile.
  SessionMetrics::FromProfile(otrProfile)
      ->RecordAndClearSessionMetrics(MetricsToRecordFlags::kNoMetrics);

  // Destroy and recreate the off-the-record Profile.
  profile->DestroyOffTheRecordProfile();
  profile->GetOffTheRecordProfile();

  for (SceneController* sceneController in sceneControllers) {
    [sceneController incognitoProfileCreated];
  }

  // This seems the best place to deem the destroying and rebuilding the
  // incognito profile to be completed.
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(
      /*in_progress=*/false);
}

- (void)willDestroyIncognitoProfile {
  // Clear the Incognito Browser and notify the TabGrid that its otrBrowser
  // will be destroyed.
  self.mainCoordinator.incognitoBrowser = nil;

  if (breadcrumbs::IsEnabled(GetApplicationContext()->GetLocalState())) {
    BreadcrumbManagerBrowserAgent::FromBrowser(self.incognitoInterface.browser)
        ->SetLoggingEnabled(false);
  }

  _incognitoWebStateObserver.reset();
  [self.browserViewWrangler willDestroyIncognitoProfile];
}

- (void)incognitoProfileCreated {
  [self.browserViewWrangler incognitoProfileCreated];

  // There should be a new URL loading browser agent for the incognito browser,
  // so set the scene URL loading service on it.
  UrlLoadingBrowserAgent::FromBrowser(self.incognitoInterface.browser)
      ->SetSceneService(_sceneURLLoadingService.get());
  _incognitoWebStateObserver = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>(
      _webStateListForwardingObserver.get());
  _incognitoWebStateObserver->Observe(
      self.incognitoInterface.browser->GetWebStateList());
  if (self.currentInterface.incognito) {
    [self activateBVCAndMakeCurrentBVCPrimary];
  }

  // Always set the new otr Browser for the tablet or grid switcher.
  // Notify the TabGrid with the new Incognito Browser.
  self.mainCoordinator.incognitoBrowser = self.incognitoInterface.browser;
}

#pragma mark - PolicyWatcherBrowserAgentObserving

- (void)policyWatcherBrowserAgentNotifySignInDisabled:
    (PolicyWatcherBrowserAgent*)policyWatcher {
  auto signinInterrupted = ^{
    policyWatcher->SignInUIDismissed();
  };

  if (self.signinCoordinator) {
    [self interruptSigninCoordinatorAnimated:YES completion:signinInterrupted];
    UMA_HISTOGRAM_BOOLEAN(
        "Enterprise.BrowserSigninIOS.SignInInterruptedByPolicy", true);
  }
}

#pragma mark - SceneUIProvider

- (UIViewController*)activeViewController {
  return self.mainCoordinator.activeViewController;
}

#pragma mark - HistoryCoordinatorDelegate

- (void)closeHistoryWithCompletion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  [self.historyCoordinator dismissWithCompletion:^{
    if (completion) {
      completion();
    }
    [weakSelf.historyCoordinator stop];
    weakSelf.historyCoordinator = nil;
  }];
}

- (void)closeHistory {
  [self closeHistoryWithCompletion:nil];
}

@end
