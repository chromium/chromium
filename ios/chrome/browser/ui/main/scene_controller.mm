// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_controller.h"

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
#import "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#import "components/breadcrumbs/core/breadcrumb_persistent_storage_manager.h"
#import "components/breadcrumbs/core/features.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/infobars/core/infobar_manager.h"
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
#import "ios/chrome/app/application_delegate/user_activity_handler.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/chrome/browser/crash_report/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/crash_loop_detection_util.h"
#import "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/crash_report/crash_restore_helper.h"
#import "ios/chrome/browser/default_browser/promo_source.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/geolocation/geolocation_logger.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/mailto_handler/mailto_handler_service.h"
#import "ios/chrome/browser/mailto_handler/mailto_handler_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/main/browser_util.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/screenshot/screenshot_delegate.h"
#import "ios/chrome/browser/sessions/session_saving_scene_agent.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/app_store_rating/app_store_rating_scene_agent.h"
#import "ios/chrome/browser/ui/app_store_rating/features.h"
#import "ios/chrome/browser/ui/appearance/appearance_customization.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_notification_infobar_delegate.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/lens_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/policy_change_commands.h"
#import "ios/chrome/browser/ui/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_scene_agent.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_scheduler.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/first_run/orientation_limiting_navigation_controller.h"
#import "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_coordinator.h"
#import "ios/chrome/browser/ui/incognito_interstitial/incognito_interstitial_coordinator_delegate.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"
#import "ios/chrome/browser/ui/main/incognito_blocker_scene_agent.h"
#import "ios/chrome/browser/ui/main/layout_guide_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_ui_provider.h"
#import "ios/chrome/browser/ui/main/ui_blocker_scene_agent.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/policy/signin_policy_scene_agent.h"
#import "ios/chrome/browser/ui/policy/user_policy_scene_agent.h"
#import "ios/chrome/browser/ui/policy/user_policy_util.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_scene_agent.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_scene_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/whats_new/promo/whats_new_scene_agent.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/url/url_util.h"
#import "ios/chrome/browser/url_loading/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/session_metrics.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_data.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

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

// Internally the NTP URL is about://newtab/.  However, with
// `url::kAboutScheme`, there's no host value, only a path.  Use this value for
// matching the NTP.
const char kAboutNewTabPath[] = "//newtab/";

bool IsNTPURL(const GURL& url) {
  // `url` can be chrome://newtab/ or about://newtab/ depending on where `url`
  // comes from (the VisibleURL chrome:// from a navigation item or the actual
  // webView url about://).  If the url is about://newtab/, there is no origin
  // to match, so instead check the scheme and the path.
  return url.DeprecatedGetOriginAsURL() == kChromeUINewTabURL ||
         (url.SchemeIs(url::kAboutScheme) && url.path() == kAboutNewTabPath);
}

void InjectNTP(Browser* browser) {
  // Don't inject an NTP for an empty web state list.
  if (!browser->GetWebStateList()->count())
    return;

  // Don't inject an NTP on an NTP.
  web::WebState* webState = browser->GetWebStateList()->GetActiveWebState();
  if (IsNTPURL(webState->GetVisibleURL()))
    return;

  // Queue up start surface with active tab.
  StartSurfaceRecentTabBrowserAgent* browser_agent =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(browser);
  // This may be nil for an incognito browser.
  if (browser_agent)
    browser_agent->SaveMostRecentTab();

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
                               PolicyWatcherBrowserAgentObserving,
                               SettingsNavigationControllerDelegate,
                               SceneUIProvider,
                               SceneURLLoadingServiceDelegate,
                               TabGridCoordinatorDelegate,
                               WebStateListObserving,
                               IncognitoInterstitialCoordinatorDelegate> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListForwardingObserver;
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
}

// Navigation View controller for the settings.
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

// The scene level component for url loading. Is passed down to
// browser state level UrlLoadingService instances.
@property(nonatomic, assign) SceneUrlLoadingService* sceneURLLoadingService;

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

// A property to track whether the QR Scanner should be started upon tab
// switcher dismissal. It can only be YES if the QR Scanner experiment is
// enabled.
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

    _sceneURLLoadingService = new SceneUrlLoadingService();
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
    tabGridCoordinator.regularThumbStripSupporting = self.mainInterface.bvc;
    tabGridCoordinator.incognitoThumbStripSupporting =
        self.incognitoInterface.bvc;
  }
  return _mainCoordinator;
}

- (id<BrowserInterface>)mainInterface {
  return self.browserViewWrangler.mainInterface;
}

- (id<BrowserInterface>)currentInterface {
  return self.browserViewWrangler.currentInterface;
}

- (id<BrowserInterface>)incognitoInterface {
  return self.browserViewWrangler.incognitoInterface;
}

- (id<BrowserInterfaceProvider>)interfaceProvider {
  return self.browserViewWrangler;
}

- (void)setStartupParameters:(AppStartupParameters*)parameters {
  _startupParameters = parameters;
  self.startupParametersAreBeingHandled = NO;
  BOOL shouldShowPromo =
      self.sceneState.appState.shouldShowDefaultBrowserPromo &&
      (parameters.postOpeningAction == NO_ACTION);
  self.sceneState.appState.shouldShowDefaultBrowserPromo = shouldShowPromo;

  if (parameters.openedViaFirstPartyScheme) {
    DefaultBrowserSceneAgent* sceneAgent =
        [DefaultBrowserSceneAgent agentFromScene:self.sceneState];
    [sceneAgent.nonModalScheduler logUserEnteredAppViaFirstPartyScheme];
    [self notifyFETAppOpenedViaFirstParty];
  }
}

- (BOOL)isPresentingSigninView {
  return self.signinCoordinator != nil;
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
    [UserActivityHandler
        performActionForShortcutItem:self.sceneState.connectionOptions
                                         .shortcutItem
                   completionHandler:nil
                           tabOpener:self
               connectionInformation:self
                  startupInformation:self.sceneState.appState.startupInformation
                   interfaceProvider:self.interfaceProvider
                           initStage:self.sceneState.appState.initStage];
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
      [self handleTabMoveActivity:activity];
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
    [UserActivityHandler
         continueUserActivity:activityWithCompletion
          applicationIsActive:NO
                    tabOpener:self
        connectionInformation:self
           startupInformation:self.sceneState.appState.startupInformation
                 browserState:self.currentInterface.browserState
                    initStage:self.sceneState.appState.initStage];
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

    [UserActivityHandler
        handleStartupParametersWithTabOpener:self
                       connectionInformation:self
                          startupInformation:self.sceneState.appState
                                                 .startupInformation
                                browserState:self.currentInterface.browserState
                                   initStage:self.sceneState.appState
                                                 .initStage];

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
  NSString* tabID = GetTabIDFromActivity(activity);

  id<BrowserInterface> interface = self.interfaceProvider.currentInterface;

  // It's expected that the current interface matches `incognito`.
  DCHECK(interface.incognito == incognito);

  // Move the tab to the current interface's browser.
  MoveTabToBrowser(tabID, interface.browser, /*destination_tab_index=*/0);
}

- (void)recordWindowCreationForSceneState:(SceneState*)sceneState {
  // Don't record window creation for single-window environments
  if (!base::ios::IsMultipleScenesSupported())
    return;

  // Don't record restored window creation.
  if (sceneState.currentOrigin == WindowActivityRestoredOrigin)
    return;

  // If there's only one connected scene, and it isn't being restored, this
  // must be the initial app launch with scenes, so don't record the window
  // creation.
  if (sceneState.appState.connectedScenes.count <= 1)
    return;

  base::UmaHistogramEnumeration(kMultiWindowOpenInNewWindowHistogram,
                                sceneState.currentOrigin);
}

- (BOOL)presentSignInAccountsViewControllerIfNecessary {
  ChromeBrowserState* browserState = self.currentInterface.browserState;
  DCHECK(browserState);
  if ([SignedInAccountsViewController
          shouldBePresentedForBrowserState:browserState]) {
    [self presentSignedInAccountsViewControllerForBrowserState:browserState];
    return YES;
  }
  return NO;
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

  [UserActivityHandler
      performActionForShortcutItem:shortcutItem
                 completionHandler:completionHandler
                         tabOpener:self
             connectionInformation:self
                startupInformation:self.sceneState.appState.startupInformation
                 interfaceProvider:self.interfaceProvider
                         initStage:self.sceneState.appState.initStage];
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
  if (IsIncognitoPolicyApplied(prefs) &&
      ![UserActivityHandler canProceedWithUserActivity:userActivity
                                           prefService:prefs]) {
    // If users request opening url in a unavailable mode, don't open the url
    // but show a toast.
    [self showToastWhenOpenExternalIntentInUnexpectedMode];
  } else {
    [UserActivityHandler
         continueUserActivity:userActivity
          applicationIsActive:sceneIsActive
                    tabOpener:self
        connectionInformation:self
           startupInformation:self.sceneState.appState.startupInformation
                 browserState:self.currentInterface.browserState
                    initStage:self.sceneState.appState.initStage];
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
  if (appInitStage < InitStageNormalUI) {
    // Nothing per-scene should happen before the app completes the global
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
      [self beginActivatingBrowser:self.mainInterface.browser
                dismissTabSwitcher:YES
                      focusOmnibox:NO];

      OpenNewTabCommand* command = [OpenNewTabCommand commandWithIncognito:NO];
      command.userInitiated = NO;
      Browser* browser = self.currentInterface.browser;
      id<ApplicationCommands> applicationHandler = HandlerForProtocol(
          browser->GetCommandDispatcher(), ApplicationCommands);
      [applicationHandler openURLInNewTab:command];
      [self finishActivatingBrowserDismissingTabSwitcher:YES];
    }
  }

  [self recordWindowCreationForSceneState:self.sceneState];

  if (self.sceneState.UIEnabled && level == SceneActivationLevelUnattached) {
    if (base::ios::IsMultipleScenesSupported()) {
      // If Multiple scenes are not supported, the session shouldn't be
      // removed as it can be used for normal restoration.
      [[PreviousSessionInfo sharedInstance]
          removeSceneSessionID:self.sceneState.sceneSessionID];
    }
    [self teardownUI];
  }
}

// Displays either the sign-in upgrade promo if it is eligible or the list
// of signed-in accounts if the user has recently updated their accounts.
- (void)tryPresentSigninModalUI {
  if ([self presentSignInAccountsViewControllerIfNecessary]) {
    // The user is already signed-in, so do not display the sign-in promo.
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

- (void)initializeUI {
  if (self.sceneState.UIEnabled) {
    return;
  }

  [self startUpChromeUI];
  self.sceneState.UIEnabled = YES;
}

// Returns YES if restore prompt can be shown.
// The restore prompt shouldn't appear if its appearance may be in conflict
// with the expected behavior by the user.
// The following cases will not show restore prompt:
//   1- New tab / Navigation startup parameters are specified.
//   2- Load URL User activity is queud.
//   3- Move tab user activity is queued.
//   4- Only incognito mode is available.
// In these cases if a restore prompt was shown, it may be dismissed immediately
// and the user will not have a chance to restore the session.
- (BOOL)shouldShowRestorePrompt {
  BOOL shouldShow = !self.startupParameters && ![self isIncognitoForced];
  if (shouldShow) {
    for (NSUserActivity* activity in self.sceneState.connectionOptions
             .userActivities) {
      if (ActivityIsTabMove(activity) || ActivityIsURLLoad(activity)) {
        shouldShow = NO;
        break;
      }
    }
  }
  return shouldShow;
}

// Starts up a single chrome window and its UI.
- (void)startUpChromeUI {
  DCHECK(!self.browserViewWrangler);
  DCHECK(self.sceneURLLoadingService);
  DCHECK(self.sceneState.appState.mainBrowserState);

  self.browserViewWrangler = [[BrowserViewWrangler alloc]
             initWithBrowserState:self.sceneState.appState.mainBrowserState
                       sceneState:self.sceneState
       applicationCommandEndpoint:self
      browsingDataCommandEndpoint:self.browsingDataCommandsHandler];

  // Ensure the main browser is created.
  Browser* mainBrowser = [self.browserViewWrangler createMainBrowser];
  CommandDispatcher* mainCommandDispatcher =
      mainBrowser->GetCommandDispatcher();

  // Add scene agents that require CommandDispatcher.
  DefaultBrowserSceneAgent* defaultBrowserAgent =
      [[DefaultBrowserSceneAgent alloc]
          initWithCommandDispatcher:mainCommandDispatcher];
  defaultBrowserAgent.nonModalScheduler.browser = mainBrowser;
  [self.sceneState addAgent:defaultBrowserAgent];
  if (defaultBrowserAgent.nonModalScheduler) {
    [self.sceneState addObserver:defaultBrowserAgent.nonModalScheduler];
  }

  // Create and start the BVC.
  [self.browserViewWrangler createMainCoordinatorAndInterface];

  // Create the inactive browser. Should be called after the main browser is
  // created (in -createMainBrowser) and restored (in
  // -createMainCoordinatorAndInterface). Even if the feature is disabled, we
  // always create the inactive browser to restore any element that have been
  // saved in the past. To avoid any tab disappearance from user perspective, we
  // move all tabs accordingly.
  [self.browserViewWrangler createInactiveBrowser];

  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(mainCommandDispatcher, ApplicationCommands);
  id<PolicyChangeCommands> policyChangeCommandsHandler =
      HandlerForProtocol(mainCommandDispatcher, PolicyChangeCommands);

  [self.sceneState
      addAgent:[[SigninPolicySceneAgent alloc]
                       initWithSceneUIProvider:self
                    applicationCommandsHandler:applicationCommandsHandler
                   policyChangeCommandsHandler:policyChangeCommandsHandler]];

  ChromeBrowserState* browserState = self.sceneState.appState.mainBrowserState;
  PrefService* prefService = browserState->GetPrefs();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);

  if (IsUserPolicyNotificationNeeded(authService, prefService)) {
    policy::UserPolicySigninService* userPolicyService =
        policy::UserPolicySigninServiceFactory::GetForBrowserState(
            browserState);
    [self.sceneState
        addAgent:[[UserPolicySceneAgent alloc]
                        initWithSceneUIProvider:self
                                    authService:authService
                     applicationCommandsHandler:applicationCommandsHandler
                                    prefService:prefService
                                    mainBrowser:mainBrowser
                                  policyService:userPolicyService]];
  }

  // Now that the main browser's command dispatcher is created and the newly
  // started UI coordinators have registered with it, inject it into the
  // PolicyWatcherBrowserAgent so it can start monitoring UI-impacting policy
  // changes.
  PolicyWatcherBrowserAgent* policyWatcherAgent =
      PolicyWatcherBrowserAgent::FromBrowser(self.mainInterface.browser);
  policyWatcherAgent->AddObserver(_policyWatcherObserverBridge.get());
  policyWatcherAgent->Initialize(policyChangeCommandsHandler);

  self.screenshotDelegate = [[ScreenshotDelegate alloc]
      initWithBrowserInterfaceProvider:self.browserViewWrangler];
  [self.sceneState.scene.screenshotService setDelegate:self.screenshotDelegate];

  // Only create the restoration helper if the session with the current session
  // id was backed up successfully.
  if (self.sceneState.appState.sessionRestorationRequired &&
      !self.sceneState.appState.startupInformation.isFirstRun) {
    if ([CrashRestoreHelper
            isBackedUpSessionID:self.sceneState.sceneSessionID
                   browserState:mainBrowser->GetBrowserState()]) {
      self.sceneState.appState.startupInformation.restoreHelper =
          [[CrashRestoreHelper alloc] initWithBrowser:mainBrowser];
    }
  }

  // If the application crashed, clear incognito state.
  if (self.sceneState.appState.postCrashAction ==
      PostCrashAction::kStashTabsAndShowNTP)
    [self clearIOSSpecificIncognitoData];

  [self createInitialUI:[self initialUIMode]];

  if ([self shouldShowRestorePrompt]) {
    [self.sceneState.appState.startupInformation
            .restoreHelper showRestorePrompt];
    self.sceneState.appState.startupInformation.restoreHelper = nil;
  }

  // Make sure the geolocation controller is created to observe permission
  // events.
  [GeolocationLogger sharedInstance];

  if (IsFullscreenPromosManagerEnabled())
    [self.sceneState
        addAgent:[[PromosManagerSceneAgent alloc]
                     initWithCommandDispatcher:mainCommandDispatcher]];
  if (IsAppStoreRatingEnabled()) {
    [self.sceneState
        addAgent:[[AppStoreRatingSceneAgent alloc]
                     initWithPromosManager:GetApplicationContext()
                                               ->GetPromosManager()]];
  }

  if (IsWhatsNewEnabled()) {
    [self.sceneState
        addAgent:[[WhatsNewSceneAgent alloc]
                     initWithPromosManager:GetApplicationContext()
                                               ->GetPromosManager()]];
  }

  // Do not gate by feature flag so it can run for enabled -> disabled
  // scenarios.
  [self.sceneState
      addAgent:[[CredentialProviderPromoSceneAgent alloc]
                   initWithPromosManager:GetApplicationContext()
                                             ->GetPromosManager()
                             prefService:self.sceneState.appState
                                             .mainBrowserState->GetPrefs()]];
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

  // If the app crashed, always launch in normal mode.
  if (self.sceneState.appState.postCrashAction ==
      PostCrashAction::kStashTabsAndShowNTP) {
    return ApplicationMode::NORMAL;
  }

  // Launch in the mode that matches the state of the scene when the application
  // was terminated. If the scene was showing the incognito UI, but there are
  // no incognito tabs open (e.g. the tab switcher was active and user closed
  // the last tab), then instead show the regular UI.

  if (self.sceneState.incognitoContentVisible &&
      !self.interfaceProvider.incognitoInterface.browser->GetWebStateList()
           ->empty()) {
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
      ->SetSceneService(self.sceneURLLoadingService);
  UrlLoadingBrowserAgent::FromBrowser(self.incognitoInterface.browser)
      ->SetSceneService(self.sceneURLLoadingService);
  // Observe the web state lists for both browsers.
  self.mainInterface.browser->GetWebStateList()->AddObserver(
      _webStateListForwardingObserver.get());
  self.incognitoInterface.browser->GetWebStateList()->AddObserver(
      _webStateListForwardingObserver.get());

  // Enables UI initializations to query the keyWindow's size.
  [self.sceneState.window makeKeyAndVisible];

  // Lazy init of mainCoordinator.
  [self.mainCoordinator start];

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
    DCHECK(base::FeatureList::IsEnabled(kRemoveCrashInfobar));
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
    [self beginActivatingBrowser:self.mainInterface.browser
              dismissTabSwitcher:YES
                    focusOmnibox:NO];
    [self finishActivatingBrowserDismissingTabSwitcher:YES];
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
  [self maybeShowDefaultBrowserPromo:self.mainInterface.browser];
}

// Notifies the Feature Engagement Tracker that an eligibility criterion has
// been met for the default browser blue dot promo.
- (void)notifyFETAppOpenedViaFirstParty {
  ChromeBrowserState* browserState = self.sceneState.appState.mainBrowserState;
  if (!browserState || browserState->IsOffTheRecord()) {
    return;
  }

  if (HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch()) {
    feature_engagement::TrackerFactory::GetForBrowserState(browserState)
        ->NotifyEvent(feature_engagement::events::kBlueDotPromoCriterionMet);
  }
}

// `YES` if Chrome is not the default browser, the app did not crash recently,
// the user never saw the promo UI and is in the correct experiment groups.
- (BOOL)potentiallyInterestedUser {
  // If skipping first run, not in Safe Mode, no post opening action and the
  // launch is not after a crash, consider showing the default browser promo.
  TabOpeningPostOpeningAction postOpeningAction =
      self.NTPActionAfterTabSwitcherDismissal;
  if (self.startupParameters) {
    postOpeningAction = self.startupParameters.postOpeningAction;
  }
  return postOpeningAction == NO_ACTION &&
         GetApplicationContext()->WasLastShutdownClean() &&
         !IsChromeLikelyDefaultBrowser() &&
         !HasUserOpenedSettingsFromFirstRunPromo();
}

- (void)maybeShowDefaultBrowserPromo:(Browser*)browser {
  ChromeBrowserState* browserState = self.sceneState.appState.mainBrowserState;
  PrefService* prefService = browserState->GetPrefs();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);

  if (self.sceneState.appState.startupInformation.isFirstRun ||
      IsUserPolicyNotificationNeeded(authService, prefService) ||
      ![self potentiallyInterestedUser]) {
    // Don't show the default browser promo when either (1) the user is going
    // through the First Run screens, (2) the user MUST see the User Policy
    // notification, OR (3) it was determined that the user isn't potentially
    // interested in that promo.
    //
    // Showing the User Policy notification has priority over showing the
    // default browser promo. Both dialogs are competing for the same time slot
    // which is after the browser startup and the browser UI is initialized.
    return;
  }

  // Don't show the default browser promo if the user is in the default browser
  // blue dot experiment.
  // TODO(crbug.com/1410229) clean-up experiment code when fully launched.
  if (!AreDefaultBrowserPromosEnabled()) {
    return;
  }

  // Show the Default Browser promo UI if the user's past behavior fits
  // the categorization of potentially interested users or if the user is
  // signed in. Do not show if it is determined that Chrome is already the
  // default browser (checked in the if enclosing this comment) or if the user
  // has already seen the promo UI. If the user was in the experiment group
  // that showed the Remind Me Later button and tapped on it, then show the
  // promo again if now is the right time.

  BOOL isSignedIn = [self isSignedIn];

  // Tailored promos take priority over general promo.
  BOOL isMadeForIOSPromoEligible =
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeMadeForIOS);
  BOOL isAllTabsPromoEligible =
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeAllTabs) &&
      isSignedIn;
  BOOL isStaySafePromoEligible =
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeStaySafe);

  BOOL isTailoredPromoEligibleUser =
      !HasUserInteractedWithTailoredFullscreenPromoBefore() &&
      (isMadeForIOSPromoEligible || isAllTabsPromoEligible ||
       isStaySafePromoEligible);
  if (isTailoredPromoEligibleUser && !UserInPromoCooldown()) {
    self.sceneState.appState.shouldShowDefaultBrowserPromo = YES;
    self.sceneState.appState.defaultBrowserPromoTypeToShow =
        MostRecentInterestDefaultPromoType(!isSignedIn);
    DCHECK(self.sceneState.appState.defaultBrowserPromoTypeToShow !=
           DefaultPromoTypeGeneral);
    return;
  }

  BOOL isGeneralPromoEligibleUser =
      !HasUserInteractedWithFullscreenPromoBefore() &&
      (IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral) ||
       isSignedIn) &&
      !UserInPromoCooldown();
  if (isGeneralPromoEligibleUser ||
      ShouldShowRemindMeLaterDefaultBrowserFullscreenPromo()) {
    self.sceneState.appState.shouldShowDefaultBrowserPromo = YES;
    self.sceneState.appState.defaultBrowserPromoTypeToShow =
        DefaultPromoTypeGeneral;
  }
}

// This method completely destroys all of the UI. It should be called when the
// scene is disconnected.
- (void)teardownUI {
  if (!self.sceneState.UIEnabled) {
    return;  // Nothing to do.
  }

  // The UI should be stopped before the models they observe are stopped.
  // SigninCoordinator teardown is performed by the `signinCompletion` on
  // termination of async events, do not add additional teardown here.
  [self.signinCoordinator
      interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
               completion:nil];

  [self.historyCoordinator stop];
  self.historyCoordinator = nil;

  // Force close the settings if open. This gives Settings the opportunity to
  // unregister observers and destroy C++ objects before the application is
  // shut down without depending on non-deterministic call to -dealloc.
  [self settingsWasDismissed];

  [_mainCoordinator stop];
  _mainCoordinator = nil;

  // Stop observing web state list changes before tearing down the web state
  // lists.
  self.mainInterface.browser->GetWebStateList()->RemoveObserver(
      _webStateListForwardingObserver.get());
  self.incognitoInterface.browser->GetWebStateList()->RemoveObserver(
      _webStateListForwardingObserver.get());

  PolicyWatcherBrowserAgent::FromBrowser(self.mainInterface.browser)
      ->RemoveObserver(_policyWatcherObserverBridge.get());

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
}

// Formats string for display on iPadOS application switcher with the
// domain of the foreground tab and the tab count. Assumes the scene is
// visible. Will return nil if there are no tabs.
- (NSString*)displayTitleForAppSwitcher {
  DCHECK(self.currentInterface.browser);
  web::WebState* webState =
      self.currentInterface.browser->GetWebStateList()->GetActiveWebState();
  if (!webState)
    return nil;

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
          ? l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_ICOGNITO_FORCED)
          : l10n_util::GetNSString(IDS_IOS_SNACKBAR_MESSAGE_ICOGNITO_DISABLED);

  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.action = action;

  [handler showSnackbarMessage:message
                withHapticType:UINotificationFeedbackTypeError];
}

- (BOOL)isIncognitoDisabled {
  return IsIncognitoModeDisabled(
      self.mainInterface.browser->GetBrowserState()->GetPrefs());
}

- (BOOL)isIncognitoForced {
  return IsIncognitoModeForced(
      self.incognitoInterface.browser->GetBrowserState()->GetPrefs());
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
  if (self.sceneState.appState.initStage <= InitStageFirstRun) {
    return NO;
  }

  if (!signin::ShouldPresentUserSigninUpgrade(
          self.sceneState.appState.mainBrowserState,
          version_info::GetVersion())) {
    return NO;
  }
  // Don't show the promo if there is a blocking task in process.
  if (self.sceneState.appState.currentUIBlocker)
    return NO;
  // Don't show the promo in Incognito mode.
  if (self.currentInterface == self.incognitoInterface)
    return NO;
  // Don't show promos if the app was launched from a URL.
  if (self.startupParameters)
    return NO;
  // Don't show the promo if the window is not active.
  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive)
    return NO;
  // Don't show the promo if the tab grid is active.
  if (self.mainCoordinator.isTabGridActive)
    return NO;
  // Don't show the promo if already presented.
  if (self.sceneState.appState.signinUpgradePromoPresentedOnce)
    return NO;
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

- (void)dismissModalDialogs {
  [self dismissModalDialogsWithCompletion:nil dismissOmnibox:YES];
}

- (void)showHistory {
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.currentInterface.viewController
                         browser:self.mainInterface.browser];
  self.historyCoordinator.loadStrategy =
      self.currentInterface.incognito ? UrlLoadStrategy::ALWAYS_IN_INCOGNITO
                                      : UrlLoadStrategy::NORMAL;
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
    [self.currentInterface.bvc userEnteredTabSwitcher];

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

// TODO(crbug.com/779791) : Remove showing settings from MainController.
- (void)showAutofillSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  if (self.settingsNavigationController)
    return;

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController autofillProfileControllerForBrowser:browser
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
  if (self.settingsNavigationController)
    return;

  UserFeedbackData* data =
      [self createUserFeedbackDataForSender:sender
                        specificProductData:specificProductData];

  Browser* browser = self.mainInterface.browser;
  id<ApplicationCommands> handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  self.settingsNavigationController =
      [SettingsNavigationController userFeedbackControllerForBrowser:browser
                                                            delegate:self
                                                    userFeedbackData:data
                                                             handler:handler];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
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
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForBrowserState(browserState);
    std::string username =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
            .email;
    if (!username.empty())
      data.currentPageSyncedUserName = base::SysUTF8ToNSString(username);
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
  self.sceneURLLoadingService->LoadUrlInNewTab(params);
}

// TODO(crbug.com/779791) : Do not pass `baseViewController` through dispatcher.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  Browser* mainBrowser = self.mainInterface.browser;

  switch (command.operation) {
    case AuthenticationOperationReauthenticate:
      self.signinCoordinator = [SigninCoordinator
          reAuthenticationCoordinatorWithBaseViewController:baseViewController
                                                    browser:mainBrowser
                                                accessPoint:command.accessPoint
                                                promoAction:command
                                                                .promoAction];
      break;
    case AuthenticationOperationSigninAndSync:
      self.signinCoordinator = [SigninCoordinator
          userSigninCoordinatorWithBaseViewController:baseViewController
                                              browser:mainBrowser
                                             identity:command.identity
                                          accessPoint:command.accessPoint
                                          promoAction:command.promoAction];
      break;
    case AuthenticationOperationSigninOnly:
      self.signinCoordinator = [SigninCoordinator
          consistencyPromoSigninCoordinatorWithBaseViewController:
              baseViewController
                                                          browser:mainBrowser
                                                      accessPoint:
                                                          command.accessPoint];
      break;
    case AuthenticationOperationAddAccount:
      self.signinCoordinator = [SigninCoordinator
          addAccountCoordinatorWithBaseViewController:baseViewController
                                              browser:mainBrowser
                                          accessPoint:command.accessPoint];
      break;
    case AuthenticationOperationForcedSigninAndSync:
      self.signinCoordinator = [SigninCoordinator
          forcedSigninCoordinatorWithBaseViewController:baseViewController
                                                browser:mainBrowser];
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
  if (self.signinCoordinator || self.settingsNavigationController)
    return;
  if (self.sceneState.appState.initStage == InitStageFirstRun) {
    // This case is possible when using force FRE flag and opening chrome
    // with accounts.google.com in the background.
    // crbug.com/1293305.
    return;
  }
  self.signinCoordinator = [SigninCoordinator
      consistencyPromoSigninCoordinatorWithBaseViewController:baseViewController
                                                      browser:self.mainInterface
                                                                  .browser
                                                  accessPoint:
                                                      signin_metrics::AccessPoint::
                                                          ACCESS_POINT_WEB_SIGNIN];
  if (!self.signinCoordinator)
    return;
  __weak SceneController* weakSelf = self;

  // Copy the URL so it can be safely captured in the block.
  GURL copiedURL = url;

  [self startSigninCoordinatorWithCompletion:^(BOOL success) {
    // If the sign-in is not successful or the scene controller is shut down do
    // not load the continuation URL.
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
  if (self.settingsNavigationController)
    return;
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
  if (!base::ios::IsMultipleScenesSupported())
    return;  // silent no-op.

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

#pragma mark - ApplicationSettingsCommands

// TODO(crbug.com/779791) : Remove show settings from MainController.
- (void)showAccountsSettingsFromViewController:
    (UIViewController*)baseViewController {
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
        showAccountsSettingsFromViewController:baseViewController];
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
        startPasswordCheckAutomatically:YES
                       showCancelButton:showCancelButton];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showSavedPasswordsSettingsAndStartPasswordCheckFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([self.signinCoordinator description]);
  [self dismissModalDialogs];
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSavedPasswordsSettingsAndStartPasswordCheckFromViewController:
            baseViewController];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController savePasswordsControllerForBrowser:browser
                                                             delegate:self
                                      startPasswordCheckAutomatically:YES
                                                     showCancelButton:NO];
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
                                                              delegate:self];
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

- (void)showSafetyCheckSettingsAndStartSafetyCheck {
  UIViewController* baseViewController = self.currentInterface.viewController;
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
            showSafetyCheckSettingsAndStartSafetyCheck];
    return;
  }
  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController =
      [SettingsNavigationController safetyCheckControllerForBrowser:browser
                                                           delegate:self];
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

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self closeSettingsUI];
}

- (void)settingsWasDismissed {
  [self.settingsNavigationController cleanUpSettings];
  self.settingsNavigationController = nil;
}

- (id<ApplicationCommands, BrowserCommands>)handlerForSettings {
  // Assume that settings always wants the dispatcher from the main BVC.
  return static_cast<id<ApplicationCommands, BrowserCommands>>(
      self.mainInterface.browser->GetCommandDispatcher());
}

- (id<ApplicationCommands>)handlerForApplicationCommands {
  // Assume that settings always wants the dispatcher from the main BVC.
  return HandlerForProtocol(self.mainInterface.browser->GetCommandDispatcher(),
                            ApplicationCommands);
}

- (id<SnackbarCommands>)handlerForSnackbarCommands {
  // Assume that settings always wants the dispatcher from the main BVC.
  return HandlerForProtocol(self.mainInterface.browser->GetCommandDispatcher(),
                            SnackbarCommands);
}

#pragma mark - TabGridCoordinatorDelegate

- (void)tabGrid:(TabGridCoordinator*)tabGrid
    shouldActivateBrowser:(Browser*)browser
           dismissTabGrid:(BOOL)dismissTabGrid
             focusOmnibox:(BOOL)focusOmnibox {
  DCHECK(dismissTabGrid || self.mainCoordinator.isThumbStripEnabled);
  [self beginActivatingBrowser:browser
            dismissTabSwitcher:dismissTabGrid
                  focusOmnibox:focusOmnibox];
}

- (void)tabGridDismissTransitionDidEnd:(TabGridCoordinator*)tabGrid {
  if (!self.sceneState.UIEnabled) {
    return;
  }
  [self finishActivatingBrowserDismissingTabSwitcher:YES];
}

- (TabGridPage)activePageForTabGrid:(TabGridCoordinator*)tabGrid {
  return self.activePage;
}

// Begins the process of activating the given current model, switching which BVC
// is suspended if necessary. If `dismissTabSwitcher` is set, the tab switcher
// will also be dismissed. Note that this means that a browser can be activated
// without closing the tab switcher (e.g. thumb strip), but dismissing the tab
// switcher requires activating a browser. The omnibox will be focused after the
// tab switcher dismissal is completed if `focusOmnibox` is YES.
- (void)beginActivatingBrowser:(Browser*)browser
            dismissTabSwitcher:(BOOL)dismissTabSwitcher
                  focusOmnibox:(BOOL)focusOmnibox {
  DCHECK(browser == self.mainInterface.browser ||
         browser == self.incognitoInterface.browser);
  DCHECK(dismissTabSwitcher || self.mainCoordinator.isThumbStripEnabled);

  self.activatingBrowser = YES;
  ApplicationMode mode = (browser == self.mainInterface.browser)
                             ? ApplicationMode::NORMAL
                             : ApplicationMode::INCOGNITO;
  [self setCurrentInterfaceForMode:mode];

  // The call to set currentBVC above does not actually display the BVC, because
  // _activatingBrowser is YES.  So: Force the BVC transition to start.
  [self displayCurrentBVCAndFocusOmnibox:focusOmnibox
                      dismissTabSwitcher:dismissTabSwitcher];
  // If the tab switcher was not dismissed, finish the activation process now.
  if (!dismissTabSwitcher) {
    [self finishActivatingBrowserDismissingTabSwitcher:NO];
  }
}

// Completes the process of activating the given browser. If necessary, also
// finishes dismissing the tab switcher, removing it from the
// screen and showing the appropriate BVC.
- (void)finishActivatingBrowserDismissingTabSwitcher:
    (BOOL)dismissingTabSwitcher {
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
    if (dismissingTabSwitcher) {
      self.modeToDisplayOnTabSwitcherDismissal = TabSwitcherDismissalMode::NONE;
      self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
      [self showTabSwitcher];
    }
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

  if (dismissingTabSwitcher) {
    self.modeToDisplayOnTabSwitcherDismissal = TabSwitcherDismissalMode::NONE;

    ProceduralBlock action = [self completionBlockForTriggeringAction:
                                       self.NTPActionAfterTabSwitcherDismissal];
    self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
    if (action) {
      action();
    }
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
    case START_LENS:
      return ^{
        [weakSelf startLens];
      };
    case FOCUS_OMNIBOX:
      return ^{
        [weakSelf focusOmnibox];
      };
    case SHOW_DEFAULT_BROWSER_SETTINGS:
      return ^{
        [weakSelf showDefaultBrowserSettings];
      };
    default:
      return nil;
  }
}

// Starts a voice search on the current BVC.
- (void)startVoiceSearchInCurrentBVC {
  // If the background (non-current) BVC is playing TTS audio, call
  // -startVoiceSearch on it to stop the TTS.
  BrowserViewController* backgroundBVC =
      self.mainInterface == self.currentInterface ? self.incognitoInterface.bvc
                                                  : self.mainInterface.bvc;
  // TODO(crbug.com/1329104): playingTTS will be removed as an API from the BVC
  // and something else will be used instead.
  if (backgroundBVC.playingTTS)
    [backgroundBVC startVoiceSearch];
  else
    [self.currentInterface.bvc startVoiceSearch];
}

- (void)startQRCodeScanner {
  if (!self.currentInterface.browser) {
    return;
  }
  id<QRScannerCommands> QRHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), QRScannerCommands);
  [QRHandler showQRScanner];
}

- (void)startLens {
  if (!self.currentInterface.browser) {
    return;
  }
  id<LensCommands> lensHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), LensCommands);
  [lensHandler
      openInputSelectionForEntrypoint:LensEntrypoint::HomeScreenWidget];
}

- (void)focusOmnibox {
  if (!self.currentInterface.browser) {
    return;
  }
  id<OmniboxCommands> omniboxCommandsHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), OmniboxCommands);
  [omniboxCommandsHandler focusOmnibox];
}

- (void)showDefaultBrowserSettings {
  if (!self.currentInterface.browser) {
    return;
  }
  id<ApplicationSettingsCommands> applicationSettingsCommandsHandler =
      HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                         ApplicationSettingsCommands);
  [applicationSettingsCommandsHandler
      showDefaultBrowserSettingsFromViewController:nil
                                      sourceForUMA:DefaultBrowserPromoSource::
                                                       kExternalIntent];
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
        base::FeatureList::IsEnabled(kIOS3PIntentsInIncognito) &&
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
  id<BrowserInterface> targetInterface =
      [self extractInterfaceBaseOnMode:targetMode];

  web::WebState* currentWebState =
      targetInterface.browser->GetWebStateList()->GetActiveWebState();

  if (currentWebState) {
    web::NavigationManager* navigation_manager =
        currentWebState->GetNavigationManager();
    // Check if the current tab is in the procress of restoration and whether it
    // is an NTP. If so, add the tabs-opening action to the
    // RestoreCompletionCallback queue so that the tabs are opened only after
    // the NTP finishes restoring. This is to avoid an edge where multiple tabs
    // are trying to open in the middle of NTP restoration, as this will cause
    // all tabs trying to load into the same NTP, causing a race condition that
    // results in wrong behavior.
    if (navigation_manager->IsRestoreSessionInProgress() &&
        IsURLNtp(currentWebState->GetVisibleURL())) {
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
        !(otrBrowser->GetWebStateList()->empty()))
      return NO;

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
  DCHECK(self.interfaceProvider);
  BOOL incognitio = mode == ApplicationMode::INCOGNITO;
  id<BrowserInterface> currentInterface =
      self.interfaceProvider.currentInterface;
  id<BrowserInterface> newInterface =
      incognitio ? self.interfaceProvider.incognitoInterface
                 : self.interfaceProvider.mainInterface;
  if (currentInterface && currentInterface == newInterface)
    return;

  // Update the snapshot before switching another application mode.  This
  // ensures that the snapshot is correct when links are opened in a different
  // application mode.
  [self updateActiveWebStateSnapshot];

  self.interfaceProvider.currentInterface = newInterface;

  if (!self.activatingBrowser)
    [self displayCurrentBVCAndFocusOmnibox:NO dismissTabSwitcher:YES];

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
    [self.interfaceProvider.currentInterface
        clearPresentedStateWithCompletion:completion
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
    targetMode = self.interfaceProvider.currentInterface.incognito
                     ? ApplicationMode::INCOGNITO
                     : ApplicationMode::NORMAL;
  } else if (tabOpeningTargetMode == ApplicationModeForTabOpening::NORMAL) {
    targetMode = ApplicationMode::NORMAL;
  } else {
    targetMode = ApplicationMode::INCOGNITO;
  }

  id<BrowserInterface> targetInterface =
      targetMode == ApplicationMode::NORMAL
          ? self.interfaceProvider.mainInterface
          : self.interfaceProvider.incognitoInterface;
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
      // Voice search, QRScanner and the omnibox are presented by the BVC.
      // They must be started after the BVC view is added in the hierarchy.
      self.NTPActionAfterTabSwitcherDismissal =
          [self.startupParameters postOpeningAction];
      [self setStartupParameters:nil];

      [self addANewTabAndPresentBrowser:targetInterface.browser
                      withURLLoadParams:urlLoadParams];
      // In this particular usage, there should be no postOpeningAction,
      // as triggering voice search while there are multiple windows opened is probably
      // a bad idea both technically and as a user experience.
      // It should be the caller duty to not set a completion if they don't need it.
      if (completion) {
        completion();
      }
    }
  } else {
    if (!self.currentInterface.viewController.presentedViewController) {
      [targetInterface.bvc expectNewForegroundTab];
    }
    [self setCurrentInterfaceForMode:targetMode];
    [self openOrReuseTabInMode:targetMode
             withUrlLoadParams:urlLoadParams
           tabOpenedCompletion:tabOpenedCompletion];
  }

  if (self.sceneState.appState.startupInformation.restoreHelper) {
    // Now that all the operations on the tabs have been done, display the
    // restore infobar if needed.
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.sceneState.appState.startupInformation
              .restoreHelper showRestorePrompt];
      self.sceneState.appState.startupInformation.restoreHelper = nil;
    });
  }
}

- (void)expectNewForegroundTabForMode:(ApplicationMode)targetMode {
  id<BrowserInterface> interface =
      targetMode == ApplicationMode::INCOGNITO
          ? self.interfaceProvider.incognitoInterface
          : self.interfaceProvider.mainInterface;
  DCHECK(interface);
  [interface.bvc expectNewForegroundTab];
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
  if (!self.currentInterface)
    return;

  WebStateList* webStateList = self.currentInterface.browser->GetWebStateList();
  web::WebState* webState = webStateList->GetActiveWebState();
  if (webState) {
    SnapshotTabHelper::FromWebState(webState)->UpdateSnapshotWithCallback(nil);
  }
}

// Checks the target BVC's current tab's URL. If this URL is chrome://newtab,
// loads `urlLoadParams` in this tab. Otherwise, open `urlLoadParams` in a new
// tab in the target BVC. `tabDisplayedCompletion` will be called on the new tab
// (if not nil).
- (void)openOrReuseTabInMode:(ApplicationMode)targetMode
           withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
         tabOpenedCompletion:(ProceduralBlock)tabOpenedCompletion {
  id<BrowserInterface> targetInterface = targetMode == ApplicationMode::NORMAL
                                             ? self.mainInterface
                                             : self.incognitoInterface;

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
      IsURLNtp(currentWebState->GetVisibleURL()) &&
      IsURLNtp(urlLoadParams.web_params.url)) {
    if (tabOpenedCompletion) {
      tabOpenedCompletion();
    }
    return;
  }

  // If the current tab isn't an NTP, open a new tab.  Be sure to use
  // -GetLastCommittedURL incase the NTP is still loading.
  if (alwaysInsertNewTab ||
      !(currentWebState && IsURLNtp(currentWebState->GetVisibleURL()))) {
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
// If `dismissTabSwitcher` is NO, then the tab switcher is not dismissed,
// although the BVC will be visible. `dismissTabSwitcher` is only used in the
// thumb strip feature.
- (void)displayCurrentBVCAndFocusOmnibox:(BOOL)focusOmnibox
                      dismissTabSwitcher:(BOOL)dismissTabSwitcher {
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
         shouldCloseTabGrid:dismissTabSwitcher
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

- (void)presentSignedInAccountsViewControllerForBrowserState:
    (ChromeBrowserState*)browserState {
  UMA_HISTOGRAM_BOOLEAN("Signin.SignedInAccountsViewImpression", true);
  id<ApplicationSettingsCommands> settingsHandler =
      HandlerForProtocol(self.mainInterface.browser->GetCommandDispatcher(),
                         ApplicationSettingsCommands);
  UIViewController* accountsViewController =
      [[SignedInAccountsViewController alloc]
          initWithBrowserState:browserState
                    dispatcher:settingsHandler];
  [[self topPresentedViewController]
      presentViewController:accountsViewController
                   animated:YES
                 completion:nil];
}

// Close Settings, or Signin or the 3rd-party intents Incognito interstitial.
- (void)closePresentedViews:(BOOL)animated
                 completion:(ProceduralBlock)completion {
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

  if (self.settingsNavigationController) {
    ProceduralBlock dismissSettings = ^() {
      [self.settingsNavigationController cleanUpSettings];
      UIViewController* presentingViewController =
          [self.settingsNavigationController presentingViewController];
      // If presentingViewController is nil it means the VC was already
      // dismissed by some other action like swiping down.
      DCHECK(presentingViewController);
      [presentingViewController dismissViewControllerAnimated:animated
                                                   completion:completion];
      self.settingsNavigationController = nil;
    };
    // `self.signinCoordinator` can be presented on top of the settings, to
    // present the Trusted Vault reauthentication `self.signinCoordinator` has
    // to be closed first.
    if (self.signinCoordinator) {
      [self interruptSigninCoordinatorAnimated:animated
                                    completion:dismissSettings];
    } else if (dismissSettings) {
      dismissSettings();
    }
  } else if (self.signinCoordinator) {
    // `self.signinCoordinator` can be presented without settings, from the
    // bookmarks or the recent tabs view.
    [self interruptSigninCoordinatorAnimated:animated completion:completion];
  } else if (self.incognitoInterstitialCoordinator) {
    [self.incognitoInterstitialCoordinator stop];
    self.incognitoInterstitialCoordinator = nil;
    completion();
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
  SigninCoordinatorInterruptAction action =
      animated ? SigninCoordinatorInterruptActionDismissWithAnimation
               : SigninCoordinatorInterruptActionDismissWithoutAnimation;

  self.dismissingSigninPromptFromExternalTrigger = YES;
  [self.signinCoordinator interruptWithAction:action completion:completion];
}

// Starts the sign-in coordinator with a default cleanup completion.
- (void)startSigninCoordinatorWithCompletion:
    (signin_ui::CompletionCallback)completion {
  DCHECK(self.signinCoordinator);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.sceneState.appState.mainBrowserState);
  AuthenticationService::ServiceStatus statusService =
      authenticationService->GetServiceStatus();
  switch (statusService) {
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy: {
      if (completion) {
        completion(/*success=*/NO);
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
        if (!weakSelf)
          return;
        __typeof(self) strongSelf = weakSelf;
        [strongSelf.signinCoordinator stop];
        strongSelf.signinCoordinator = nil;
        uiBlocker.reset();

        if (completion) {
          completion(result == SigninCoordinatorResultSuccess);
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

// Called when a WebState is removed. Triggers the switcher view when the last
// WebState is closed on a device that uses the switcher.
- (void)webStateList:(WebStateList*)notifiedWebStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  // Do nothing on initialization.
  if (!self.currentInterface.browser)
    return;

  if (notifiedWebStateList->empty()) {
    if (webState->GetBrowserState()->IsOffTheRecord()) {
      [self lastIncognitoTabClosed];
    } else {
      [self lastRegularTabClosed];
    }
  }
}

#pragma mark - IncognitoInterstitialCoordinatorDelegate

- (void)shouldStopIncognitoInterstitial:
    (IncognitoInterstitialCoordinator*)incognitoInterstitial {
  DCHECK(incognitoInterstitial == self.incognitoInterstitialCoordinator);
  [self closePresentedViews:YES completion:nil];
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
  if (browsingDataRemover && browsingDataRemover->IsRemoving())
    return;

  self.interfaceProvider.mainInterface.userInteractionEnabled = YES;
  self.interfaceProvider.incognitoInterface.userInteractionEnabled = YES;
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

- (id<BrowserInterface>)extractInterfaceBaseOnMode:
    (ApplicationModeForTabOpening)targetMode {
  DCHECK(targetMode != ApplicationModeForTabOpening::UNDETERMINED);
  ApplicationMode applicationMode;

  if (targetMode == ApplicationModeForTabOpening::CURRENT) {
    applicationMode = self.interfaceProvider.currentInterface.incognito
                          ? ApplicationMode::INCOGNITO
                          : ApplicationMode::NORMAL;
  } else if (targetMode == ApplicationModeForTabOpening::NORMAL) {
    applicationMode = ApplicationMode::NORMAL;
  } else {
    applicationMode = ApplicationMode::INCOGNITO;
  }

  id<BrowserInterface> targetInterface =
      applicationMode == ApplicationMode::NORMAL
          ? self.interfaceProvider.mainInterface
          : self.interfaceProvider.incognitoInterface;

  return targetInterface;
}

#pragma mark - TabGrid helpers

// Returns the page that should be active in the TabGrid.
- (TabGridPage)activePage {
  if (self.currentInterface.browser == self.incognitoInterface.browser)
    return TabGridPageIncognitoTabs;
  return TabGridPageRegularTabs;
}

// Adds a new tab to the `browser` based on `urlLoadParams` and then presents
// it.
- (void)addANewTabAndPresentBrowser:(Browser*)browser
                  withURLLoadParams:(const UrlLoadParams&)urlLoadParams {
  TabInsertionBrowserAgent::FromBrowser(browser)->InsertWebState(
      urlLoadParams.web_params, nil, false, browser->GetWebStateList()->count(),
      /*in_background=*/false, /*inherit_opener=*/false,
      /*should_show_start_surface=*/false,
      /*should_skip_new_tab_animation=*/urlLoadParams.from_external);
  [self beginActivatingBrowser:browser dismissTabSwitcher:YES focusOmnibox:NO];
}

#pragma mark - Handling of destroying the incognito BrowserState

// The incognito BrowserState should be closed when the last incognito tab is
// closed (i.e. if there are other incognito tabs open in another Scene, the
// BrowserState must not be destroyed).
- (BOOL)shouldDestroyAndRebuildIncognitoBrowserState {
  ChromeBrowserState* mainBrowserState =
      self.sceneState.appState.mainBrowserState;
  if (!mainBrowserState->HasOffTheRecordChromeBrowserState())
    return NO;

  ChromeBrowserState* otrBrowserState =
      mainBrowserState->GetOffTheRecordChromeBrowserState();
  DCHECK(otrBrowserState);

  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(otrBrowserState);
  for (Browser* browser : browserList->AllIncognitoBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    if (!webStateList->empty())
      return NO;
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

  // Delete all the remaining sessions. This is asynchronous, but will happen
  // after all pending saves, if any, have completed. There is a risk of a
  // race-condition with loading them, but as -incognitoBrowserStateCreated
  // does not load the session, the only risk is if the application were to
  // crash before the deletion could complete (in which case the user may
  // see the previous state of the app before closing the last incognito tab).
  [[SessionServiceIOS sharedService]
      deleteAllSessionFilesInDirectory:otrBrowserState->GetStatePath()
                            completion:base::DoNothing()];

  // Record off-the-record metrics before detroying the BrowserState.
  SessionMetrics::FromBrowserState(otrBrowserState)
      ->RecordAndClearSessionMetrics(MetricsToRecordFlags::kNoMetrics);

  // Destroy and recreate the off-the-record BrowserState.
  mainBrowserState->DestroyOffTheRecordChromeBrowserState();
  mainBrowserState->GetOffTheRecordChromeBrowserState();

  for (SceneController* sceneController in sceneControllers) {
    [sceneController incognitoBrowserStateCreated];
  }

  if (base::FeatureList::IsEnabled(breadcrumbs::kLogBreadcrumbs)) {
    BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
        mainBrowserState->GetOffTheRecordChromeBrowserState());
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

  if (base::FeatureList::IsEnabled(breadcrumbs::kLogBreadcrumbs)) {
    BreadcrumbManagerBrowserAgent::FromBrowser(self.incognitoInterface.browser)
        ->SetLoggingEnabled(false);
  }

  self.incognitoInterface.browser->GetWebStateList()->RemoveObserver(
      _webStateListForwardingObserver.get());
  [self.browserViewWrangler willDestroyIncognitoBrowserState];
}

- (void)incognitoBrowserStateCreated {
  [self.browserViewWrangler incognitoBrowserStateCreated];

  // There should be a new URL loading browser agent for the incognito browser,
  // so set the scene URL loading service on it.
  UrlLoadingBrowserAgent::FromBrowser(self.incognitoInterface.browser)
      ->SetSceneService(self.sceneURLLoadingService);
  self.incognitoInterface.browser->GetWebStateList()->AddObserver(
      _webStateListForwardingObserver.get());

  if (self.currentInterface.incognito) {
    [self activateBVCAndMakeCurrentBVCPrimary];
  }

  // Always set the new otr Browser for the tablet or grid switcher.
  // Notify the TabGrid with the new Incognito Browser.
  self.mainCoordinator.incognitoBrowser = self.incognitoInterface.browser;
  self.mainCoordinator.incognitoThumbStripSupporting =
      self.incognitoInterface.bvc;
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

@end
