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
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_browser_agent.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_loop_detection_util.h"
#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/geolocation/model/geolocation_logger.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/intents/user_activity_browser_agent.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"
#import "ios/chrome/browser/main/model/browser_util.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/promos_manager_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/screenshot/model/screenshot_delegate.h"
#import "ios/chrome/browser/sessions/session_saving_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
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
#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/ui/app_store_rating/app_store_rating_scene_agent.h"
#import "ios/chrome/browser/ui/app_store_rating/features.h"
#import "ios/chrome/browser/ui/appearance/appearance_customization.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_notification_infobar_delegate.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_scene_agent.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/promo/omnibox_position_choice_scene_agent.h"
#import "ios/chrome/browser/ui/first_run/orientation_limiting_navigation_controller.h"
#import "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/history/history_coordinator_delegate.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_coordinator.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_coordinator_delegate.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/main/default_browser_promo_scene_agent.h"
#import "ios/chrome/browser/ui/main/incognito_blocker_scene_agent.h"
#import "ios/chrome/browser/ui/main/ui_blocker_scene_agent.h"
#import "ios/chrome/browser/ui/main/wrangled_browser.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/policy/signin_policy_scene_agent.h"
#import "ios/chrome/browser/ui/policy/user_policy_scene_agent.h"
#import "ios/chrome/browser/ui/policy/user_policy_util.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_scene_agent.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_scene_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/whats_new/promo/whats_new_scene_agent.h"
#import "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/page_placeholder_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/session_metrics.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_data.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"
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

// TODO(crbug.com/1244632): Use the Authentication Service sign-in status API
// instead of this when available.
bool IsSigninForcedByPolicy() {
  BrowserSigninMode policy_mode = static_cast<BrowserSigninMode>(
      GetApplicationContext()->GetLocalState()->GetInteger(
          prefs::kBrowserSigninPolicy));
  return policy_mode == BrowserSigninMode::kForced;
}

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
  web::WebState::CreateParams create_params(browser->GetBrowserState());
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(create_params);
  std::vector<std::unique_ptr<web::NavigationItem>> items;
  std::unique_ptr<web::NavigationItem> item(web::NavigationItem::Create());
  item->SetURL(GURL(kChromeUINewTabURL));
  items.push_back(std::move(item));
  web_state->GetNavigationManager()->Restore(0, std::move(items));
  NewTabPageTabHelper::CreateForWebState(web_state.get());
  NewTabPageTabHelper::FromWebState(web_state.get())->SetShowStartSurface(true);
  int index = browser->GetWebStateList()->count();
  browser->GetWebStateList()->InsertWebState(index, std::move(web_state),
                                             WebStateList::INSERT_ACTIVATE,
                                             WebStateOpener());
}

}  // namespace

@interface SceneController () <AppStateObserver,
                               HistoryCoordinatorDelegate,
                               IncognitoInterstitialCoordinatorDelegate,
                               PasswordCheckupCoordinatorDelegate,
                               PolicyWatcherBrowserAgentObserving,
                               SettingsNavigationControllerDelegate,
                               SceneUIProvider,
                               SceneURLLoadingServiceDelegate,
                               SignedInAccountsViewControllerDelegate,
                               TabGridCoordinatorDelegate,
                               WebStateListObserving> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListForwardingObserver;
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
  // View controller presents the signed in accounts when they have changed
  // while the application was in background.
  SignedInAccountsViewController* _signedInAccountsVC;
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
  // browser state level UrlLoadingService instances.
  std::unique_ptr<SceneUrlLoadingService> _sceneURLLoadingService;

  // Map recording the number of tabs in WebStateList before the batch
  // operation started.
  std::map<WebStateList*, int> _tabCountBeforeBatchOperation;
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
// TODO(crbug.com/560296):  Provide a general solution for handling mutually
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
    [_sceneState.appState addObserver:self];

    _sceneURLLoadingService = std::make_unique<SceneUrlLoadingService>();
    _sceneURLLoadingService->SetDelegate(self);

    _webStateListForwardingObserver =
        std::make_unique<WebStateListObserverBridge>(self);

    _policyWatcherObserverBridge =
        std::make_unique<PolicyWatcherBrowserAgentObserverBridge>(self);

    // Add agents.
    [_sceneState addAgent:[[UIBlockerSceneAgent alloc] init]];
    [_sceneState addAgent:[[IncognitoBlockerSceneAgent alloc] init]];
    [_sceneState
        addAgent:[[IncognitoReauthSceneAgent alloc]
                     initWithReauthModule:[[ReauthenticationModule alloc]
                                              init]]];
    [_sceneState addAgent:[[StartSurfaceSceneAgent alloc] init]];
    [_sceneState addAgent:[[SessionSavingSceneAgent alloc] init]];
    [_sceneState addAgent:[[LayoutGuideSceneAgent alloc] init]];
  }
  return self;
}

#pragma mark - Setters and getters

- (id<BrowsingDataCommands>)browsingDataCommandsHandler {
  return HandlerForProtocol(self.sceneState.appState.appCommandDispatcher,
                            BrowsingDataCommands);
}

- (TabGridCoordinator*)mainCoordinator {
  if (!_mainCoordinator) {
    // Lazily create the main coordinator.
    TabGridCoordinator* tabGridCoordinator = [[TabGridCoordinator alloc]
                     initWithWindow:self.sceneState.window
         applicationCommandEndpoint:self
        browsingDataCommandEndpoint:self.browsingDataCommandsHandler
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
    [self notifyFETAppOpenedViaFirstParty];
  }
}

- (BOOL)isTabGridVisible {
  return self.mainCoordinator.isTabGridActive;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  AppState* appState = self.sceneState.appState;
  [self transitionToSceneActivationLevel:level appInitStage:appState.initStage];
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
    [self
        openTabFromLaunchWithParams:params
                 startupInformation:self.sceneState.appState.startupInformation
                           appState:self.sceneState.appState];
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
  if (sceneState.appState.connectedScenes.count <= 1) {
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
  if (self.sceneState.appState.initStage <= InitStageNormalUI ||
      !self.currentInterface.browserState) {
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

  if (self.sceneState.appState.initStage <= InitStageNormalUI ||
      !self.currentInterface.browserState) {
    // Don't handle the intent if the browser UI objects aren't yet initialized.
    // This is the case when the app is in safe mode or may be the case when the
    // app is going through an odd sequence of lifecyle events (shouldn't happen
    // but happens somehow), see crbug.com/1211006 for more details.
    return;
  }

  BOOL sceneIsActive = [self canHandleIntents];
  self.sceneState.startupHadExternalIntent = YES;

  PrefService* prefs = self.currentInterface.browserState->GetPrefs();
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

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  [self transitionToSceneActivationLevel:self.sceneState.activationLevel
                            appInitStage:appState.initStage];
}

#pragma mark - private

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
                                                   displayAsHalfSheet:NO
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

// A sink for appState:didTransitionFromInitStage: and
// sceneState:transitionedToActivationLevel: events. Discussion: the scene
// controller cares both about the app and the scene init stages. This method is
// called from both observer callbacks and allows to handle all the transitions
// in one place.
- (void)transitionToSceneActivationLevel:(SceneActivationLevel)level
                            appInitStage:(InitStage)appInitStage {
  if (level == SceneActivationLevelDisconnected) {
    //  The scene may become disconnected at any time. In that case, any UI that
    //  was already set-up should be torn down.
    [self teardownUI];
  }
  if (appInitStage < InitStageNormalUI) {
    // Nothing else per-scene should happen before the app completes the global
    // setup, like executing Safe mode, or creating the main BrowserState.
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
      appInitStage == InitStageFinal) {
    [self tryPresentSigninModalUI];

    [self handleExternalIntents];

    if (!initializingUIInColdStart && self.mainCoordinator.isTabGridActive &&
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

  [self recordWindowCreationForSceneState:self.sceneState];

  if (self.sceneState.UIEnabled && level <= SceneActivationLevelDisconnected) {
    if (base::ios::IsMultipleScenesSupported()) {
      // If Multiple scenes are not supported, the session shouldn't be
      // removed as it can be used for normal restoration.
      [[PreviousSessionInfo sharedInstance]
          removeSceneSessionID:self.sceneState.sceneSessionID];
    }
  }
}

// Displays either the sign-in upgrade promo if it is eligible or the list
// of signed-in accounts if the user has recently updated their accounts. These
// two sign-in modal dialog are mutually exclusive (one is presented when the
// user is signed in to Chrome, the other when the user is signed out of
// Chrome).
- (void)tryPresentSigninModalUI {
  [self presentSignInAccountsViewControllerIfNecessary];
  if (_signedInAccountsVC) {
    // The sign-in upgrade promo cannot be shown when the signed-in accounts
    // view controller in shown (signed-in accounts view controller in only
    // presented when the user is signed in to Chrome).
    return;
  }

  // If the sign-in promo is not eligible, return immediately.
  if (![self shouldPresentSigninUpgradePromo]) {
    return;
  }

  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.mainInterface.browser->GetBrowserState());
  // The sign-in promo should not be presented if there is no identities.
  id<SystemIdentity> defaultIdentity =
      accountManagerService->GetDefaultIdentity();
  DCHECK(defaultIdentity);

  using CapabilityResult = SystemIdentityManager::CapabilityResult;
  SystemIdentityManager* system_identity_manager =
      GetApplicationContext()->GetSystemIdentityManager();

  // Asynchronously checks whether the default identity can display extended
  // sync promos and displays the sign-in promo if possible.
  __weak SceneController* weakSelf = self;
  base::Time fetch_start = base::Time::Now();
  system_identity_manager->CanOfferExtendedSyncPromos(
      defaultIdentity, base::BindOnce(^(CapabilityResult result) {
        base::TimeDelta fetch_duration = (base::Time::Now() - fetch_start);
        base::UmaHistogramTimes(
            "Signin.AccountCapabilities.GetFromSystemLibraryDuration."
            "SigninUpgradePromo",
            fetch_duration);
        if (fetch_duration > signin::GetWaitThresholdForCapabilities() ||
            result != CapabilityResult::kTrue) {
          return;
        }
        [weakSelf presentSigninUpgradePromo];
      }));
}

// Present the sign-in accounts view if the accounts have changed while in
// background.
- (void)presentSignInAccountsViewControllerIfNecessary {
  ChromeBrowserState* browserState = self.currentInterface.browserState;
  DCHECK(browserState);

  if (_signedInAccountsVC ||
      ![SignedInAccountsViewController
          shouldBePresentedForBrowserState:browserState]) {
    return;
  }

  // The signed-in view controller needs to be presented.
  id<ApplicationSettingsCommands> settingsHandler =
      HandlerForProtocol(self.mainInterface.browser->GetCommandDispatcher(),
                         ApplicationSettingsCommands);
  _signedInAccountsVC = [[SignedInAccountsViewController alloc]
      initWithBrowserState:browserState
                dispatcher:settingsHandler];
  _signedInAccountsVC.delegate = self;
  [[self topPresentedViewController] presentViewController:_signedInAccountsVC
                                                  animated:YES
                                                completion:nil];
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
  DCHECK(self.sceneState.appState.mainBrowserState);

  SceneState* sceneState = self.sceneState;
  ChromeBrowserState* browserState = sceneState.appState.mainBrowserState;
  self.browserViewWrangler = [[BrowserViewWrangler alloc]
             initWithBrowserState:browserState
                       sceneState:sceneState
       applicationCommandEndpoint:self
      browsingDataCommandEndpoint:self.browsingDataCommandsHandler];

  // Create and start the BVC.
  [self.browserViewWrangler createMainCoordinatorAndInterface];
  Browser* mainBrowser = self.browserViewWrangler.mainInterface.browser;
  CommandDispatcher* mainCommandDispatcher =
      mainBrowser->GetCommandDispatcher();

  PromosManager* promosManager =
      PromosManagerFactory::GetForBrowserState(browserState);

  // Add scene agents that require CommandDispatcher.
  DefaultBrowserPromoSceneAgent* defaultBrowserAgent =
      [[DefaultBrowserPromoSceneAgent alloc]
          initWithCommandDispatcher:mainCommandDispatcher];
  defaultBrowserAgent.promosManager = promosManager;
  [sceneState addAgent:defaultBrowserAgent];
  [sceneState
      addAgent:[[NonModalDefaultBrowserPromoSchedulerSceneAgent alloc] init]];

  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(mainCommandDispatcher, ApplicationCommands);
  id<PolicyChangeCommands> policyChangeCommandsHandler =
      HandlerForProtocol(mainCommandDispatcher, PolicyChangeCommands);

  [sceneState
      addAgent:[[SigninPolicySceneAgent alloc]
                       initWithSceneUIProvider:self
                    applicationCommandsHandler:applicationCommandsHandler
                   policyChangeCommandsHandler:policyChangeCommandsHandler]];

  PrefService* prefService = browserState->GetPrefs();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);

  policy::UserCloudPolicyManager* userPolicyManager =
      browserState->GetUserCloudPolicyManager();
  if (IsUserPolicyNotificationNeeded(authService, prefService,
                                     userPolicyManager)) {
    policy::UserPolicySigninService* userPolicyService =
        policy::UserPolicySigninServiceFactory::GetForBrowserState(
            browserState);
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

  [self.browserViewWrangler loadSession];
  [self createInitialUI:[self initialUIMode]];

  // Make sure the geolocation controller is created to observe permission
  // events.
  [GeolocationLogger sharedInstance];

  [sceneState addAgent:[[PromosManagerSceneAgent alloc]
                           initWithCommandDispatcher:mainCommandDispatcher]];

  if (IsAppStoreRatingEnabled()) {
    [sceneState addAgent:[[AppStoreRatingSceneAgent alloc]
                             initWithPromosManager:promosManager]];
  }

  [sceneState addAgent:[[WhatsNewSceneAgent alloc]
                           initWithPromosManager:promosManager]];

  // Do not gate by feature flag so it can run for enabled -> disabled
  // scenarios.
  [sceneState addAgent:[[CredentialProviderPromoSceneAgent alloc]
                           initWithPromosManager:promosManager
                                     prefService:browserState->GetPrefs()]];

  if (IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kAppLaunch)) {
    [sceneState addAgent:[[OmniboxPositionChoiceSceneAgent alloc]
                             initWithPromosManager:promosManager
                                   forBrowserState:browserState]];
  }
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
  DCHECK(self.sceneState.appState.mainBrowserState);

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
  [self.mainCoordinator setActivePage:[self activePage]];

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

// Notifies the Feature Engagement Tracker that an eligibility criterion has
// been met for the default browser blue dot promo.
- (void)notifyFETAppOpenedViaFirstParty {
  ChromeBrowserState* browserState = self.sceneState.appState.mainBrowserState;
  if (!browserState || browserState->IsOffTheRecord()) {
    return;
  }

  if (HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch()) {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForBrowserState(browserState);

    tracker->NotifyEvent(feature_engagement::events::kBlueDotPromoCriterionMet);
    tracker->NotifyEvent(
        feature_engagement::events::kDefaultBrowserVideoPromoConditionsMet);
  }
}

- (void)teardownUI {
  [_signedInAccountsVC teardownUI];
  _signedInAccountsVC.delegate = nil;
  _signedInAccountsVC = nil;

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

  // TODO(crbug.com/1229306): Consider moving this at the beginning of
  // teardownUI to indicate that the UI is about to be torn down and that the
  // dependencies depending on the browser UI models has to be cleaned up
  // agent).
  self.sceneState.UIEnabled = NO;

  [[SessionSavingSceneAgent agentFromScene:self.sceneState]
      saveSessionsIfNeeded];
  [self.browserViewWrangler shutdown];
  self.browserViewWrangler = nil;

  [self.sceneState.appState removeObserver:self];
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

  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.action = action;

  [handler showSnackbarMessage:message
                withHapticType:UINotificationFeedbackTypeError];
}

- (BOOL)isIncognitoDisabled {
  return IsIncognitoModeDisabled(
      self.mainInterface.browser->GetBrowserState()->GetPrefs());
}

// YES if incognito mode is forced by enterprise policy.
- (BOOL)isIncognitoForced {
  return IsIncognitoModeForced(
      self.incognitoInterface.browser->GetBrowserState()->GetPrefs());
}

// Returns 'YES' if the tabID from the given `activity` is valid.
- (BOOL)isTabActivityValid:(NSUserActivity*)activity {
  web::WebStateID tabID = GetTabIDFromActivity(activity);

  ChromeBrowserState* browserState = self.currentInterface.browserState;
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(browserState);
  const std::set<Browser*>& browsers = self.currentInterface.incognito
                                           ? browserList->AllIncognitoBrowsers()
                                           : browserList->AllRegularBrowsers();

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
          self.sceneState.appState.mainBrowserState,
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
- (void)presentSigninUpgradePromo {
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

  if (self.sceneState.appState.initStage <= InitStageFirstRun) {
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

  if (_signedInAccountsVC) {
    // Don't handle intents if the user has the Signed In Accounts view
    // controller presented on the screen.
    return NO;
  }

  return YES;
}

- (BOOL)isSignedIn {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.sceneState.appState.mainBrowserState);
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
  [self.mainCoordinator prepareToShowTabGrid];
}

- (void)displayRegularTabSwitcherInGridLayout {
  [self displayTabSwitcherForcingRegularTabs:YES];
}

- (void)displayTabSwitcherInGridLayout {
  [self displayTabSwitcherForcingRegularTabs:NO];
}

- (void)displayTabSwitcherForcingRegularTabs:(BOOL)forcing {
  DCHECK(!self.mainCoordinator.isTabGridActive);
  if (!self.isProcessingVoiceSearchCommand) {

    if (forcing && self.currentInterface.incognito) {
      [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
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
    [weakSelf presentReportAnIssueViewController:baseViewController
                                          sender:sender
                             specificProductData:specificProductData];
  });
}

- (void)presentReportAnIssueViewController:(UIViewController*)baseViewController
                                    sender:(UserFeedbackSender)sender
                       specificProductData:(NSDictionary<NSString*, NSString*>*)
                                               specificProductData {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (self.settingsNavigationController) {
    return;
  }

  UserFeedbackData* data =
      [self createUserFeedbackDataForSender:sender
                        specificProductData:specificProductData];

  Browser* browser = self.mainInterface.browser;
  id<ApplicationCommands> handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);

  if (ios::provider::CanUseStartUserFeedbackFlow()) {
    UserFeedbackConfiguration* configuration =
        [[UserFeedbackConfiguration alloc] init];
    configuration.data = data;
    configuration.handler = handler;
    configuration.singleSignOnService =
        GetApplicationContext()->GetSSOService();

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
}

- (UserFeedbackData*)createUserFeedbackDataForSender:(UserFeedbackSender)sender
                                 specificProductData:
                                     (NSDictionary<NSString*, NSString*>*)
                                         specificProductData {
  UserFeedbackData* data = [[UserFeedbackData alloc] init];
  data.origin = sender;
  data.currentPageIsIncognito = self.currentInterface.incognito;

  CGFloat scale = 0.0;
  if (!self.mainCoordinator.isTabGridActive) {
    web::WebState* webState =
        self.currentInterface.browser->GetWebStateList()->GetActiveWebState();
    if (webState) {
      // Record URL of browser tab that is currently showing
      GURL url = webState->GetVisibleURL();
      std::u16string urlText = url_formatter::FormatUrl(url);
      data.currentPageDisplayURL = base::SysUTF16ToNSString(urlText);
    }
  } else {
    // For screenshots of the tab switcher we need to use a scale of 1.0 to
    // avoid spending too much time since the tab switcher can have lots of
    // subviews.
    scale = 1.0;
  }

  UIView* lastView = self.mainCoordinator.activeViewController.view;
  DCHECK(lastView);
  data.currentPageScreenshot = CaptureView(lastView, scale);

  ChromeBrowserState* browserState = self.currentInterface.browserState;
  if (browserState->IsOffTheRecord()) {
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

// TODO(crbug.com/779791) : Do not pass `baseViewController` through dispatcher.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController {
  if (command.skipIfUINotAvaible &&
      (baseViewController.presentedViewController ||
       ![self isTabAvailableToPresentViewController])) {
    // Make sure the UI is available to present the sign-in view.
    return;
  }
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
    case AuthenticationOperation::kSigninAndSyncReauth:
      self.signinCoordinator = [SigninCoordinator
          signinAndSyncReauthCoordinatorWithBaseViewController:
              baseViewController
                                                       browser:mainBrowser
                                                   accessPoint:command
                                                                   .accessPoint
                                                   promoAction:
                                                       command.promoAction];
      break;
    case AuthenticationOperation::kSigninAndSync:
      self.signinCoordinator = [SigninCoordinator
          userSigninCoordinatorWithBaseViewController:baseViewController
                                              browser:mainBrowser
                                             identity:command.identity
                                          accessPoint:command.accessPoint
                                          promoAction:command.promoAction];
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
                                                browser:mainBrowser];
      break;
    case AuthenticationOperation::kSigninAndSyncWithTwoScreens:
      self.signinCoordinator = [SigninCoordinator
          twoScreensSigninCoordinatorWithBaseViewController:baseViewController
                                                    browser:mainBrowser
                                                accessPoint:command.accessPoint
                                                promoAction:command
                                                                .promoAction];
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
    showTrustedVaultReauthForFetchKeysFromViewController:
        (UIViewController*)viewController
                                                 trigger:
                                                     (syncer::
                                                          TrustedVaultUserActionTriggerForUMA)
                                                         trigger {
  [self
      showTrustedVaultDialogFromViewController:viewController
                                        intent:
                                            SigninTrustedVaultDialogIntentFetchKeys
                                       trigger:trigger];
}

- (void)
    showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
        (UIViewController*)viewController
                                                              trigger:
                                                                  (syncer::
                                                                       TrustedVaultUserActionTriggerForUMA)
                                                                      trigger {
  [self
      showTrustedVaultDialogFromViewController:viewController
                                        intent:
                                            SigninTrustedVaultDialogIntentDegradedRecoverability
                                       trigger:trigger];
}

- (void)showWebSigninPromoFromViewController:
            (UIViewController*)baseViewController
                                         URL:(const GURL&)url {
  // Do not display the web sign-in promo if there is any UI on the screen.
  if (baseViewController.presentedViewController ||
      ![self isTabAvailableToPresentViewController]) {
    return;
  }
  if (!signin::ShouldPresentWebSignin(self.mainInterface.browserState)) {
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
  SigninNotificationInfoBarDelegate::Create(
      infoBarManager, self.mainInterface.browser->GetBrowserState(), self,
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
  if (!baseViewController) {
    baseViewController = self.currentInterface.viewController;
  }

  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (self.settingsNavigationController) {
    return;
  }
  [[DeferredInitializationRunner sharedInstance]
      runBlockIfNecessary:kPrefObserverInit];

  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController =
      [SettingsNavigationController mainSettingsControllerForBrowser:browser
                                                            delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
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
    PrefService* prefs = self.mainInterface.browserState->GetPrefs();
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

// Returns YES if the current Tab is available to present a view controller.
- (BOOL)isTabAvailableToPresentViewController {
  if (self.signinCoordinator) {
    return NO;
  }
  if (self.settingsNavigationController) {
    return NO;
  }
  if (self.sceneState.appState.initStage <= InitStageFirstRun) {
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

#pragma mark - ApplicationSettingsCommands

// TODO(crbug.com/779791) : Remove show settings from MainController.
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
    NOTREACHED();
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

// TODO(crbug.com/779791) : Remove Google services settings from MainController.
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

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
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

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
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

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showSavedPasswordsSettingsFromViewController:
            (UIViewController*)baseViewController
                                    showCancelButton:(BOOL)showCancelButton {
  if (!baseViewController) {
    // TODO(crbug.com/779791): Don't pass base view controller through
    // dispatched command.
    baseViewController = self.currentInterface.viewController;
  }
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  [self dismissModalDialogsWithCompletion:nil];
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
                        showCancelButton:(BOOL)showCancelButton {
  UIViewController* baseViewController = self.currentInterface.viewController;
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showPasswordDetailsForCredential:credential
                        showCancelButton:showCancelButton];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      passwordDetailsControllerForBrowser:browser
                                 delegate:self
                               credential:credential
                         showCancelButton:showCancelButton];
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

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
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

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
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

- (void)showCreditCardDetails:(const autofill::CreditCard*)creditCard {
  UIViewController* baseViewController = self.currentInterface.viewController;
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showCreditCardDetails:creditCard];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      autofillCreditCardEditControllerForBrowser:browser
                                        delegate:self
                                      creditCard:creditCard];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showDefaultBrowserSettingsFromViewController:
            (UIViewController*)baseViewController
                                        sourceForUMA:
                                            (DefaultBrowserPromoSource)source {
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

// Displays the Safety Check (via Settings) for `referrer`. `showHalfSheet`
// determines whether the Safety Check will be displayed as a half-sheet, or
// full-page modal.
- (void)showAndStartSafetyCheckInHalfSheet:(BOOL)showHalfSheet
                                  referrer:
                                      (password_manager::PasswordCheckReferrer)
                                          referrer {
  UIViewController* baseViewController = self.currentInterface.viewController;

  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showAndStartSafetyCheckInHalfSheet:showHalfSheet
                                  referrer:referrer];
    return;
  }

  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController = [SettingsNavigationController
      safetyCheckControllerForBrowser:browser
                             delegate:self
                   displayAsHalfSheet:showHalfSheet
                             referrer:referrer];

  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showSafeBrowsingSettings {
  UIViewController* baseViewController = self.currentInterface.viewController;
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

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self closeSettingsUI];
}

- (void)settingsWasDismissed {
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

- (TabGridPage)activePageForTabGrid:(TabGridCoordinator*)tabGrid {
  return self.activePage;
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
                      DefaultBrowserPromoSource::kExternalIntent];
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
        [weakSelf.mainCoordinator showTabGrid];
      };
    case SET_CHROME_DEFAULT_BROWSER:
      return ^{
        [weakSelf showDefaultBrowserSettingsWithSourceForUMA:
                      DefaultBrowserPromoSource::kExternalIntent];
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
        [weakSelf
            showAndStartSafetyCheckInHalfSheet:NO
                                      referrer:password_manager::
                                                   PasswordCheckReferrer::
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
                      DefaultBrowserPromoSource::kExternalAction];
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
    (DefaultBrowserPromoSource)sourceForUMA {
  if (!self.currentInterface.browser) {
    return;
  }
  id<ApplicationSettingsCommands> applicationSettingsCommandsHandler =
      HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                         ApplicationSettingsCommands);
  [applicationSettingsCommandsHandler
      showDefaultBrowserSettingsFromViewController:nil
                                      sourceForUMA:sourceForUMA];
}

- (void)startPasswordSearch {
  Browser* browser = self.currentInterface.browser;
  if (!browser) {
    return;
  }
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(
          browser->GetBrowserState());
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kPasswordManagerWidgetPromoUsed);
  }

  id<ApplicationSettingsCommands> applicationSettingsCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(),
                         ApplicationSettingsCommands);
  [applicationSettingsCommandsHandler showPasswordSearchPage];
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

  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                         ApplicationCommands);
  [applicationCommandsHandler showCreditCardSettings];
}

- (void)openClearBrowsingDataDialog {
  if (!self.currentInterface.browser) {
    return;
  }

  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                         ApplicationCommands);
  [applicationCommandsHandler showClearBrowsingDataSettings];
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
    PrefService* prefs = self.mainInterface.browserState->GetPrefs();
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

  ApplicationModeForTabOpening targetMode =
      incognitoMode ? ApplicationModeForTabOpening::INCOGNITO
                    : ApplicationModeForTabOpening::NORMAL;
  WrangledBrowser* targetInterface =
      [self extractInterfaceBaseOnMode:targetMode];

  web::WebState* currentWebState =
      targetInterface.browser->GetWebStateList()->GetActiveWebState();

  if (currentWebState) {
    web::NavigationManager* navigation_manager =
        currentWebState->GetNavigationManager();
    // Check if the current tab is in the process of restoration and whether it
    // is an NTP. If so, add the tabs-opening action to the
    // RestoreCompletionCallback queue so that the tabs are opened only after
    // the NTP finishes restoring. This is to avoid an edge where multiple tabs
    // are trying to open in the middle of NTP restoration, as this will cause
    // all tabs trying to load into the same NTP, causing a race condition that
    // results in wrong behavior.
    if (navigation_manager->IsRestoreSessionInProgress() &&
        IsUrlNtp(currentWebState->GetVisibleURL())) {
      navigation_manager->AddRestoreCompletionCallback(base::BindOnce(^{
        [self
            dismissModalDialogsWithCompletion:^{
              [weakSelf openMultipleTabsWithURLs:copyURLs
                                 inIncognitoMode:incognitoMode
                                      completion:completion];
            }
                               dismissOmnibox:dismissOmnibox];
      }));
      return;
    }
  }

  [self
      dismissModalDialogsWithCompletion:^{
        [weakSelf openMultipleTabsWithURLs:copyURLs
                           inIncognitoMode:incognitoMode
                                completion:completion];
      }
                         dismissOmnibox:dismissOmnibox];
}

- (void)openTabFromLaunchWithParams:(URLOpenerParams*)params
                 startupInformation:(id<StartupInformation>)startupInformation
                           appState:(AppState*)appState {
  if (params) {
    [URLOpener
          handleLaunchOptions:params
                    tabOpener:self
        connectionInformation:self
           startupInformation:startupInformation
                     appState:appState
                  prefService:self.currentInterface.browserState->GetPrefs()];
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
  MailtoHandlerServiceFactory::GetForBrowserState(
      self.currentInterface.browserState)
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
                                     trigger:
                                         (syncer::
                                              TrustedVaultUserActionTriggerForUMA)
                                             trigger {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  Browser* mainBrowser = self.mainInterface.browser;
  self.signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordinatorWithBaseViewController:
          viewController
                                                            browser:mainBrowser
                                                             intent:intent
                                                            trigger:trigger];
  [self startSigninCoordinatorWithCompletion:nil];
}

// Close Settings, or Signin or the 3rd-party intents Incognito interstitial.
- (void)closePresentedViews:(BOOL)animated
                 completion:(ProceduralBlock)completion {
  // If the Incognito interstitial is active, stop it.
  [self.incognitoInterstitialCoordinator stop];
  self.incognitoInterstitialCoordinator = nil;

  __weak __typeof(self) weakSelf = self;
  BOOL resetSigninState = self.signinCoordinator != nil;
  completion = ^{
    __typeof(self) strongSelf = weakSelf;
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
      [weakSelf.settingsNavigationController cleanUpSettings];
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
      weakSelf.settingsNavigationController = nil;
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
  // TODO(crbug.com/754642): Implement TopPresentedViewControllerFrom()
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
      AuthenticationServiceFactory::GetForBrowserState(
          self.sceneState.appState.mainBrowserState);
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
      NOTREACHED() << "Status service: " << static_cast<int>(statusService);
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

#pragma mark - SignedInAccountsViewControllerDelegate

- (void)signedInAccountsViewControllerIsDismissed:
    (SignedInAccountsViewController*)signedInAccountsViewController {
  CHECK_EQ(_signedInAccountsVC, signedInAccountsViewController);
  [_signedInAccountsVC teardownUI];
  _signedInAccountsVC.delegate = nil;
  _signedInAccountsVC = nil;
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

  [self.passwordCheckupCoordinator stop];

  self.passwordCheckupCoordinator.delegate = nil;
  self.passwordCheckupCoordinator = nil;
}

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  [self closeSettingsUI];
}

#pragma mark - Helpers for web state list events

// Called when the last incognito tab was closed.
- (void)lastIncognitoTabClosed {
  // If no other window has incognito tab, then destroy and rebuild the
  // BrowserState. Otherwise, just do the state transition animation.
  if ([self shouldDestroyAndRebuildIncognitoBrowserState]) {
    // Incognito browser state cannot be deleted before all the requests are
    // deleted. Queue empty task on IO thread and destroy the BrowserState
    // when the task has executed, again verifying that no incognito tabs are
    // present. When an incognito tab is moved between browsers, there is
    // a point where the tab isn't attached to any web state list. However, when
    // this queued cleanup step executes, the moved tab will be attached, so
    // the cleanup shouldn't proceed.

    auto cleanup = ^{
      if ([self shouldDestroyAndRebuildIncognitoBrowserState]) {
        [self destroyAndRebuildIncognitoBrowserState];
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
// deleting the browser state.
- (void)clearIOSSpecificIncognitoData {
  DCHECK(self.sceneState.appState.mainBrowserState
             ->HasOffTheRecordChromeBrowserState());
  ChromeBrowserState* otrBrowserState =
      self.sceneState.appState.mainBrowserState
          ->GetOffTheRecordChromeBrowserState();
  [self.browsingDataCommandsHandler
      removeBrowsingDataForBrowserState:otrBrowserState
                             timePeriod:browsing_data::TimePeriod::ALL_TIME
                             removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                        completionBlock:^{
                          [self activateBVCAndMakeCurrentBVCPrimary];
                        }];
}

- (void)activateBVCAndMakeCurrentBVCPrimary {
  // If there are pending removal operations, the activation will be deferred
  // until the callback is received.
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForBrowserStateIfExists(
          self.currentInterface.browserState);
  if (browsingDataRemover && browsingDataRemover->IsRemoving()) {
    return;
  }

  self.mainInterface.userInteractionEnabled = YES;
  self.incognitoInterface.userInteractionEnabled = YES;
  [self.currentInterface setPrimary:YES];
}

// Shows the tab switcher UI.
- (void)showTabSwitcher {
  DCHECK(self.mainCoordinator);
  [self.mainCoordinator setActivePage:self.activePage];
  [self.mainCoordinator setActiveMode:TabGridModeNormal];
  [self.mainCoordinator showTabGrid];
}

- (void)openURLContexts:(NSSet<UIOpenURLContext*>*)URLContexts {
  if (self.sceneState.appState.initStage <= InitStageNormalUI ||
      !self.currentInterface.browserState) {
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
                  prefService:self.currentInterface.browserState->GetPrefs()
                    initStage:self.sceneState.appState.initStage];
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

// Returns the page that should be active in the TabGrid.
- (TabGridPage)activePage {
  if (self.currentInterface.browser == self.incognitoInterface.browser) {
    return TabGridPageIncognitoTabs;
  }
  return TabGridPageRegularTabs;
}

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

#pragma mark - Handling of destroying the incognito BrowserState

// The incognito BrowserState should be closed when the last incognito tab is
// closed (i.e. if there are other incognito tabs open in another Scene, the
// BrowserState must not be destroyed).
- (BOOL)shouldDestroyAndRebuildIncognitoBrowserState {
  ChromeBrowserState* mainBrowserState =
      self.sceneState.appState.mainBrowserState;
  if (!mainBrowserState->HasOffTheRecordChromeBrowserState()) {
    return NO;
  }

  ChromeBrowserState* otrBrowserState =
      mainBrowserState->GetOffTheRecordChromeBrowserState();
  DCHECK(otrBrowserState);

  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(otrBrowserState);
  for (Browser* browser : browserList->AllIncognitoBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    if (!webStateList->empty()) {
      return NO;
    }
  }

  return YES;
}

// Destroys and rebuilds the incognito BrowserState. This will inform all the
// other SceneController to destroy state tied to the BrowserState and to
// recreate it.
- (void)destroyAndRebuildIncognitoBrowserState {
  // This seems the best place to mark the start of destroying the incognito
  // browser state.
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(
      /*in_progress=*/true);

  [self clearIOSSpecificIncognitoData];

  ChromeBrowserState* mainBrowserState =
      self.sceneState.appState.mainBrowserState;
  DCHECK(mainBrowserState->HasOffTheRecordChromeBrowserState());
  ChromeBrowserState* otrBrowserState =
      mainBrowserState->GetOffTheRecordChromeBrowserState();

  NSMutableArray<SceneController*>* sceneControllers =
      [[NSMutableArray alloc] init];
  for (SceneState* sceneState in [self.sceneState.appState connectedScenes]) {
    SceneController* sceneController = sceneState.controller;
    // In some circumstances, the scene state may still exist while the
    // corresponding scene controller has been deallocated.
    // (see crbug.com/1142782).
    if (sceneController) {
      [sceneControllers addObject:sceneController];
    }
  }

  for (SceneController* sceneController in sceneControllers) {
    [sceneController willDestroyIncognitoBrowserState];
  }

  // Record off-the-record metrics before detroying the BrowserState.
  SessionMetrics::FromBrowserState(otrBrowserState)
      ->RecordAndClearSessionMetrics(MetricsToRecordFlags::kNoMetrics);

  // Destroy and recreate the off-the-record BrowserState.
  mainBrowserState->DestroyOffTheRecordChromeBrowserState();
  mainBrowserState->GetOffTheRecordChromeBrowserState();

  for (SceneController* sceneController in sceneControllers) {
    [sceneController incognitoBrowserStateCreated];
  }

  // This seems the best place to deem the destroying and rebuilding the
  // incognito browser state to be completed.
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(
      /*in_progress=*/false);
}

- (void)willDestroyIncognitoBrowserState {
  // Clear the Incognito Browser and notify the TabGrid that its otrBrowser
  // will be destroyed.
  self.mainCoordinator.incognitoBrowser = nil;

  if (breadcrumbs::IsEnabled(GetApplicationContext()->GetLocalState())) {
    BreadcrumbManagerBrowserAgent::FromBrowser(self.incognitoInterface.browser)
        ->SetLoggingEnabled(false);
  }

  _incognitoWebStateObserver.reset();
  [self.browserViewWrangler willDestroyIncognitoBrowserState];
}

- (void)incognitoBrowserStateCreated {
  [self.browserViewWrangler incognitoBrowserStateCreated];

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

@end
