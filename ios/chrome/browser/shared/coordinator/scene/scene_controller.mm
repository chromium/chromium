// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"

#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/i18n/message_formatter.h"
#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/breadcrumbs/core/breadcrumbs_status.h"
#import "components/data_sharing/public/data_sharing_utils.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/url_formatter/url_formatter.h"
#import "components/version_info/version_info.h"
#import "components/web_resource/web_resource_pref_names.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/app_store_rating/model/app_store_rating_scene_agent.h"
#import "ios/chrome/browser/app_store_rating/model/features.h"
#import "ios/chrome/browser/authentication/signin/fullscreen_promo/model/fullscreen_signin_promo_scene_agent.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_authentication_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_signout_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/features.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_browser_agent.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_scene_agent.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/browser/docking_promo/model/docking_promo_scene_agent.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/geolocation/model/geolocation_manager.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_promo_scene_agent.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intents/model/user_activity_browser_agent.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/main/ui_bundled/browser_lifecycle_manager.h"
#import "ios/chrome/browser/main/ui_bundled/default_browser_promo_scene_agent.h"
#import "ios/chrome/browser/main/ui_bundled/incognito_blocker_scene_agent.h"
#import "ios/chrome/browser/main/ui_bundled/ui_blocker_scene_agent.h"
#import "ios/chrome/browser/main/ui_bundled/wrangled_browser.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/picture_in_picture/model/picture_in_picture_scene_agent.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_policy_scene_agent.h"
#import "ios/chrome/browser/policy/ui_bundled/signin_policy_scene_agent.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_scene_agent.h"
#import "ios/chrome/browser/promos_manager/public/utils.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_entry_point.h"
#import "ios/chrome/browser/scene/coordinator/scene_coordinator.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/screenshot/model/screenshot_delegate.h"
#import "ios/chrome/browser/sessions/model/session_saving_scene_agent.h"
#import "ios/chrome/browser/share_extension/model/share_extension_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller+OTRProfileDeletion.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/task_updater_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/url_context.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_scene_agent.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/whats_new/coordinator/promo/whats_new_scene_agent.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_image_transcoder/java_script_image_transcoder.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Feature to control whether Search Intents (Widgets, Application
// Shortcuts menu) forcibly open a new tab, rather than reusing an
// existing NTP. See http://crbug.com/1363375 for details.
BASE_FEATURE(kForceNewTabForIntentSearch, base::FEATURE_DISABLED_BY_DEFAULT);

// Used to update the current BVC mode if a new tab is added while the tab
// switcher view is being dismissed.  This is different than ApplicationMode in
// that it can be set to `NONE` when not in use.
enum class TabSwitcherDismissalMode { NONE, NORMAL, INCOGNITO };

// Key of the UMA IOS.MultiWindow.OpenInNewWindow histogram.
const char kMultiWindowOpenInNewWindowHistogram[] =
    "IOS.MultiWindow.OpenInNewWindow";

// Histogram key used to log the number of contexts to open that the app
// received.
const char kContextsToOpen[] = "IOS.NumberOfContextsToOpen";

// Enum for IOS.NumberOfContextsToOpen histogram.
// Keep in sync with "ContextsToOpen" in tools/metrics/histograms/enums.xml.
enum class ContextsToOpen {
  kNoContexts = 0,
  kOneContext = 1,
  kMoreThanOneContext = 2,
  kMoreThanOneContextWithAccountChange = 3,
  kMaxValue = kMoreThanOneContextWithAccountChange,
};

// TODO(crbug.com/40788009): Use the Authentication Service sign-in status API
// instead of this when available.
bool IsSigninForcedByPolicy() {
  BrowserSigninMode policy_mode = static_cast<BrowserSigninMode>(
      GetApplicationContext()->GetLocalState()->GetInteger(
          prefs::kBrowserSigninPolicy));
  return policy_mode == BrowserSigninMode::kForced;
}

// TODO(crbug.com/429353384): Can InjectNTP be factored into another file?
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

}  // namespace

// TODO(crbug.com/429355979): Order and group methods by interface.
// TODO(crbug.com/429354805): Add method comments(!)

@interface SceneController () <AuthenticationServiceObserving,
                               ProfileStateObserver,
                               SceneUIHandler,
                               SceneUIProvider,
                               SceneURLLoadingServiceDelegate,
                               TabGridCoordinatorDelegate> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListForwardingObserver;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _incognitoWebStateObserver;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _mainWebStateObserver;

  // The scene level component for url loading. Is passed down to
  // profile level UrlLoadingService instances.
  std::unique_ptr<SceneUrlLoadingService> _sceneURLLoadingService;

  // Map recording the number of tabs in WebStateList before the batch
  // operation started.
  std::map<WebStateList*, int> _tabCountBeforeBatchOperation;

  // JavaScript image transcoder to locally re-encode images to search.
  std::unique_ptr<web::JavaScriptImageTranscoder> _imageTranscoder;

  // A property to track the image to search after dismissing the tab switcher.
  // This is used to ensure that the image search is only triggered when the BVC
  // is active.
  NSData* _imageSearchData;
  // Observer for auth service status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;
  BOOL _handleExternalIntentsInProgress;
}

// Coordinates the creation of PDF screenshots with the window's content.
@property(nonatomic, strong) ScreenshotDelegate* screenshotDelegate;

// If not NONE, the current BVC should be switched to this BVC on completion
// of tab switcher dismissal.
@property(nonatomic, assign)
    TabSwitcherDismissalMode modeToDisplayOnTabSwitcherDismissal;

// A property to track which action to perform after dismissing the tab
// switcher. This is used to ensure certain post open actions that are
// presented by the BVC to only be triggered when the BVC is active.
@property(nonatomic, readwrite)
    TabOpeningPostOpeningAction NTPActionAfterTabSwitcherDismissal;

// The main coordinator to manage the main view controller. This property should
// not be accessed before the browser has started up to the FOREGROUND stage.
@property(nonatomic, strong) SceneCoordinator* mainCoordinator;

// YES while activating a new browser (often leading to dismissing the tab
// switcher.
@property(nonatomic, assign) BOOL activatingBrowser;

// YES if the scene has been backgrounded since it has last been
// SceneActivationLevelForegroundActive.
@property(nonatomic, assign) BOOL backgroundedSinceLastActivated;

// Manages the browser lifecycle.
@property(nonatomic, strong) BrowserLifecycleManager* browserLifecycleManager;

// The state of the scene controlled by this object.
@property(nonatomic, weak, readonly) SceneState* sceneState;

// The profile of the current scene.
@property(nonatomic, readonly) ProfileIOS* profile;

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
  }
  return self;
}

- (void)dealloc {
  CHECK(!_authServiceObserverBridge, base::NotFatalUntil::M145);
  CHECK(!self.browserLifecycleManager, base::NotFatalUntil::M152);
}

- (void)setProfileState:(ProfileState*)profileState {
  DCHECK(!_sceneState.profileState);

  // Connect the ProfileState with the SceneState.
  _sceneState.profileState = profileState;
  [profileState sceneStateConnected:_sceneState];

  // Add agents. They may depend on the ProfileState, so they need to be
  // created after it has been connected to the SceneState.
  [self addProfileStateDependentAgents];

  // Start observing the ProfileState. This needs to happen after the agents
  // as this may result in creation of the UI which can access to the agents.
  [profileState addObserver:self];
}

#pragma mark - Setters and getters

// TODO(crbug.com/429347474): Get rid of BrowserProviderInterface
- (WrangledBrowser*)mainInterface {
  return self.browserLifecycleManager.mainInterface;
}

- (ProfileIOS*)profile {
  return self.sceneState.profileState.profile;
}

- (WrangledBrowser*)currentInterface {
  return self.browserLifecycleManager.currentInterface;
}

- (WrangledBrowser*)incognitoInterface {
  return self.browserLifecycleManager.incognitoInterface;
}

- (id<BrowserProviderInterface>)browserProviderInterface {
  return self.browserLifecycleManager;
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

  if (parameters.openedViaShareExtensionScheme) {
    [self handleExternalIntents];
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

- (void)signinDidEnd:(SceneState*)sceneState {
  if (IsSigninForcedByPolicy()) {
    // Handle intents after sign-in is done when the forced sign-in policy
    // is enabled.
    [self handleExternalIntents];
  }
}

- (BOOL)handleExternalIntents {
  // TODO(crbug.com/462018636): Remove once the startup refactore is done.
  // Early return when a recursive call is done, while the fuction is still
  // being executed.
  if (_handleExternalIntentsInProgress) {
    return NO;
  }
  base::AutoReset<BOOL> autoResetHandleExternalIntents(
      &_handleExternalIntentsInProgress, YES);
  if (![self canHandleIntents]) {
    return NO;
  }
  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(self.currentInterface.browser);

  NSMutableSet<UIOpenURLContext*>* contexts =
      [NSMutableSet setWithSet:self.sceneState.connectionOptions.URLContexts];
  [contexts unionSet:self.sceneState.URLContextsToOpen];
  self.sceneState.URLContextsToOpen = nil;

  if ([self multipleAccountSwitchesRequired:contexts]) {
    // If more than one context require a potental account change only open the
    // first context and discard the others to avoid looping between acocunt
    // changes.
    NSEnumerator<UIOpenURLContext*>* enumerator = [contexts objectEnumerator];
    contexts = [NSMutableSet setWithObject:[enumerator nextObject]];
    base::UmaHistogramEnumeration(
        kContextsToOpen, ContextsToOpen::kMoreThanOneContextWithAccountChange);
  }

  // Find the first context that requires an account change.
  URLContext* firstContextForAccountChange =
      [self findContextRequiringAccountChange:contexts];
  // Perform profile switching if needed.
  if ([self changeProfileForContext:firstContextForAccountChange
                           contexts:contexts
                            openURL:NO]) {
    return YES;
  }

  // Handle URL opening from
  // `UIWindowSceneDelegate scene:willConnectToSession:options:`.
  for (UIOpenURLContext* context in contexts) {
    URLOpenerParams* params =
        [[URLOpenerParams alloc] initWithUIOpenURLContext:context];
    [self openTabFromLaunchWithParams:params
                   startupInformation:self.sceneState.profileState.appState
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
            setApplicationMode:ApplicationModeForTabOpening::INCOGNITO
          forceApplicationMode:YES];
    } else if ([self isIncognitoDisabled]) {
      [self.startupParameters
            setApplicationMode:ApplicationModeForTabOpening::NORMAL
          forceApplicationMode:YES];
    }

    userActivityBrowserAgent->RouteToCorrectTab();

    // Show a toast if the browser is opened in an unexpected mode.
    if (self.startupParameters.isUnexpectedMode) {
      userActivityBrowserAgent
          ->ShowToastWhenOpenExternalIntentInUnexpectedMode();
    }
  }
  return NO;
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
  // Only process the event if the profile is ready.
  if (sceneState.profileState.initStage >= ProfileInitStage::kFinal) {
    [self handleURLContextsToOpen];
  }
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

  // Perform the action in incognito when only incognito mode is available.
  if ([self isIncognitoForced]) {
    [self.startupParameters
          setApplicationMode:ApplicationModeForTabOpening::INCOGNITO
        forceApplicationMode:YES];
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

  PrefService* prefs = self.currentInterface.profile->GetPrefs();
  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(self.currentInterface.browser);
  if (IsIncognitoPolicyApplied(prefs) &&
      !userActivityBrowserAgent->ProceedWithUserActivity(userActivity)) {
    // If users request opening url in a unavailable mode, don't open the url
    // but show a toast.
    userActivityBrowserAgent->ShowToastWhenOpenExternalIntentInUnexpectedMode();
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

// If sign-in is disabled, switch to the personal profile and sign-out.
- (void)signoutIfNeeded {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(
          self.sceneState.profileState.profile);
  if (authenticationService->SigninEnabled()) {
    return;
  }
  // If sign-in is disabled, switch to personal profile and sign-out.
  if (!IsPersonalProfile(self.profile)) {
    auto signoutSource = signin_metrics::ProfileSignout::kPrefChanged;
    ChangeProfileContinuation continuation =
        CreateChangeProfileSignoutContinuation(
            signoutSource, /*force_snackbar_over_toolbar=*/false,
            /*should_record_metrics=*/false, /*snackbar_message =*/nil,
            base::DoNothing());
    signin::SwitchToPersonalProfile(self.sceneState,
                                    ChangeProfileReason::kManagedAccountSignOut,
                                    std::move(continuation));
    return;
  }
  if (![self isSignedIn]) {
    authenticationService->SignOut(signin_metrics::ProfileSignout::kPrefChanged,
                                   nil);
  }
}

- (void)handleURLContextsToOpen {
  if (self.sceneState.URLContextsToOpen.count == 0) {
    base::UmaHistogramEnumeration(kContextsToOpen, ContextsToOpen::kNoContexts);
    return;
  }
  ContextsToOpen contextInfo = self.sceneState.URLContextsToOpen.count == 1
                                   ? ContextsToOpen::kOneContext
                                   : ContextsToOpen::kMoreThanOneContext;
  base::UmaHistogramEnumeration(kContextsToOpen, contextInfo);

  NSSet<UIOpenURLContext*>* contexts = self.sceneState.URLContextsToOpen;
  if ([self multipleAccountSwitchesRequired:contexts]) {
    // If more than one context require a potental account change only open the
    // first context and discard the others to avoid looping between acocunt
    // changes.
    NSEnumerator<UIOpenURLContext*>* enumerator = [contexts objectEnumerator];
    contexts = [NSSet setWithObject:[enumerator nextObject]];
    base::UmaHistogramEnumeration(
        kContextsToOpen, ContextsToOpen::kMoreThanOneContextWithAccountChange);
  }
  self.sceneState.URLContextsToOpen = nil;

  // Find the first context that requires an account change.
  URLContext* context = [self findContextRequiringAccountChange:contexts];
  // Perform profile switching if needed.
  if ([self changeProfileForContext:context contexts:contexts openURL:YES]) {
    // Don't open the URLs if the profile was changed.
    return;
  }

  [self openURLContexts:contexts];
}

// Returns YES if a profile change was triggered.
- (BOOL)changeProfileForContext:(URLContext*)context
                       contexts:(NSSet<UIOpenURLContext*>*)contexts
                        openURL:(BOOL)openURL {
  if (!context) {
    return NO;
  }

  // Perform profile switching if needed.
  id<ChangeProfileCommands> changeProfileHandler = HandlerForProtocol(
      self.sceneState.profileState.appState.appCommandDispatcher,
      ChangeProfileCommands);

  std::optional<std::string> profileName;

  if ([context.gaiaID.ToNSString() isEqualToString:app_group::kNoAccount]) {
    // Use the personal profile name if there is no GaiaID (this happens in
    // the sign-out scenario).
    profileName = GetApplicationContext()
                      ->GetProfileManager()
                      ->GetProfileAttributesStorage()
                      ->GetPersonalProfileName();
  } else {
    profileName = GetApplicationContext()
                      ->GetAccountProfileMapper()
                      ->FindProfileNameForGaiaID(context.gaiaID);
  }

  if (!profileName.has_value()) {
    return NO;
  }

  const std::string& oldProfileName =
      self.sceneState.profileState.profile->GetProfileName();
  if (oldProfileName == profileName) {
    // In this case there will be no profile change, just an account change,
    // always open the URL in the continuation in this scenario.
    openURL = YES;
  }
  ChangeProfileReason reason;
  if ([self shareExtensionURLEligibleForAccountChange:context.context.URL]) {
    reason = ChangeProfileReason::kSwitchAccountsFromShareExtension;
  } else {
    reason = ChangeProfileReason::kSwitchAccountsFromWidget;
  }

  [changeProfileHandler
      changeProfile:*profileName
           forScene:self.sceneState
             reason:reason
       continuation:CreateChangeProfileAuthenticationContinuation(
                        context, contexts, openURL)];
  return YES;
}

- (BOOL)multipleAccountSwitchesRequired:(NSSet<UIOpenURLContext*>*)URLContexts {
  if (URLContexts.count == 1) {
    return NO;
  }

  // Store the number of URLs that may require an account change.
  int accountChanges = 0;
  for (UIOpenURLContext* context : URLContexts) {
    std::string newGaia;
    if (net::GetValueForKeyInQuery(net::GURLWithNSURL(context.URL),
                                   app_group::kGaiaIDQueryItemName, &newGaia)) {
      accountChanges++;
    }
  }
  return accountChanges > 1 ? YES : NO;
}

- (BOOL)widgetURLEligibleForAccountChange:(NSURL*)URL {
  return [URL.scheme isEqualToString:@"chromewidgetkit"];
}

- (BOOL)shareExtensionURLEligibleForAccountChange:(NSURL*)URL {
  return [URL.path
      isEqualToString:
          [NSString
              stringWithFormat:@"/%s",
                               app_group::kChromeAppGroupXCallbackCommand]];
}

- (URLContext*)findContextRequiringAccountChange:
    (NSSet<UIOpenURLContext*>*)URLContexts {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile->GetOriginalProfile());
  CoreAccountInfo primaryAccount =
      identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  for (UIOpenURLContext* context : URLContexts) {
    // Check that this URL is coming from a widget.
    if (!([self widgetURLEligibleForAccountChange:context.URL] ||
          [self shareExtensionURLEligibleForAccountChange:context.URL])) {
      continue;
    }
    std::string newGaia;

    // Continue if the URL does not contain a gaia.
    if (!net::GetValueForKeyInQuery(net::GURLWithNSURL(context.URL),
                                    app_group::kGaiaIDQueryItemName,
                                    &newGaia)) {
      continue;
    }
    GaiaId newGaiaID(newGaia);

    // Only switch account if the gaia in the widget is different from the gaia
    // in the app.
    if (primaryAccount.gaia == newGaiaID) {
      continue;
    }

    if ([newGaiaID.ToNSString() isEqualToString:app_group::kNoAccount] &&
        !primaryAccount.gaia.empty()) {
      return [[URLContext alloc] initWithContext:context
                                          gaiaID:newGaiaID
                                            type:AccountSwitchType::kSignOut];
    }
    if (newGaiaID != primaryAccount.gaia &&
        ![newGaiaID.ToNSString() isEqualToString:app_group::kNoAccount]) {
      return [[URLContext alloc] initWithContext:context
                                          gaiaID:newGaiaID
                                            type:AccountSwitchType::kSignIn];
    }
  }
  return nil;
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
  }

  // When the scene transitions to inactive (such as when it's being shown in
  // the OS app-switcher), update the title for display on iPadOS.
  if (level == SceneActivationLevelForegroundInactive) {
    self.sceneState.scene.title = [self displayTitleForAppSwitcher];
  }

  if (level == SceneActivationLevelForegroundActive &&
      profileInitStage == ProfileInitStage::kFinal) {
    if (!IsFullscreenSigninPromoManagerMigrationEnabled()) {
      [self tryPresentFullscreenSigninPromo];
    }

    if ([self handleExternalIntents]) {
      // handleExternalIntents may change profile, don't execute code below if
      // profile was changed.
      return;
    }

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
      id<SceneCommands> sceneHandler =
          HandlerForProtocol(browser->GetCommandDispatcher(), SceneCommands);
      [sceneHandler openURLInNewTab:command];
      [self finishActivatingBrowserDismissingTabSwitcher];
    }

    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForProfile(
            self.sceneState.profileState.profile);
    _authServiceObserverBridge =
        std::make_unique<AuthenticationServiceObserverBridge>(
            authenticationService, self);
    // The user may be signed-in while sign-in is disabled. In which case,
    // we must sign the user out, potentially switching profile.
    [self signoutIfNeeded];
  }
  if (level == SceneActivationLevelBackground) {
    [self recordWindowCreationForSceneState:self.sceneState];
  }
}

- (void)initializeUI {
  if (self.sceneState.UIEnabled) {
    return;
  }

  if (tests_hook::ShouldLoadMinimalAppUI()) {
    tests_hook::LoadMinimalAppUI(self.sceneState.window);
  } else {
    [self startUpChromeUI];
  }
  self.sceneState.UIEnabled = YES;
}

// Starts up a single chrome window and its UI.
- (void)startUpChromeUI {
  DCHECK(!self.browserLifecycleManager);
  DCHECK(_sceneURLLoadingService.get());
  DCHECK(self.profile);

  SceneState* sceneState = self.sceneState;
  ProfileIOS* profile = self.profile;

  _mainCoordinator = [[SceneCoordinator alloc] initWithTabOpener:self];
  _mainCoordinator.UIHandler = self;
  _mainCoordinator.tabGridDelegate = self;
  _mainCoordinator.sceneURLLoadingService = _sceneURLLoadingService.get();

  self.browserLifecycleManager =
      [[BrowserLifecycleManager alloc] initWithProfile:profile
                                            sceneState:sceneState
                                         sceneEndpoint:_mainCoordinator
                                      settingsEndpoint:_mainCoordinator];

  // Create and start the BVC.
  [self.browserLifecycleManager createMainCoordinatorAndInterface];
  [_mainCoordinator setBrowsersFromProvider:self.browserLifecycleManager];

  [self addAgents];

  self.screenshotDelegate = [[ScreenshotDelegate alloc]
      initWithBrowserProviderInterface:self.browserLifecycleManager];
  [sceneState.scene.screenshotService setDelegate:self.screenshotDelegate];

  [self activateBVCAndMakeCurrentBVCPrimary];
  [self.browserLifecycleManager loadSession];
  [self createInitialUI:[self initialUIMode]];

  // Make sure the GeolocationManager is created to observe permission events.
  [GeolocationManager sharedInstance];
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

  if (self.sceneState.incognitoState.incognitoContentVisible &&
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

  [_mainCoordinator start];

  // Enables UI initializations to query the keyWindow's size. Do this after
  // `mainCoordinator start` as it sets self.window.rootViewController to work
  // around crbug.com/850387, causing a flicker if -makeKeyAndVisible has been
  // called.
  [self.sceneState.window makeKeyAndVisible];

  if (!self.sceneState.profileState.startupInformation.isFirstRun) {
    [self reconcileEulaAsAccepted];
  }

  Browser* browser = (launchMode == ApplicationMode::INCOGNITO)
                         ? self.incognitoInterface.browser
                         : self.mainInterface.browser;

  // Inject a NTP before setting the interface, which will trigger a load of
  // the current webState.
  if (self.sceneState.profileState.appState.postCrashAction ==
      PostCrashAction::kShowNTPWithReturnToTab) {
    InjectNTP(browser);
  }

  tests_hook::InjectFakeTabsInBrowser(browser);

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
    id<SceneCommands> sceneHandler = HandlerForProtocol(
        currentBrowser->GetCommandDispatcher(), SceneCommands);
    [sceneHandler openURLInNewTab:command];
  }
}

- (void)teardownUI {
  // The UI should be stopped before the models they observe are stopped.
  [_mainCoordinator stop];
  _mainCoordinator = nil;

  _incognitoWebStateObserver.reset();
  _mainWebStateObserver.reset();
  _authServiceObserverBridge.reset();

  // TODO(crbug.com/40778288): Consider moving this at the beginning of
  // teardownUI to indicate that the UI is about to be torn down and that the
  // dependencies depending on the browser UI models has to be cleaned up
  // agent).
  self.sceneState.UIEnabled = NO;

  [self.browserLifecycleManager shutdown];
  self.browserLifecycleManager = nil;

  [self.sceneState.profileState removeObserver:self];
  _sceneURLLoadingService.reset();

  _imageTranscoder = nullptr;
}

// Formats string for display on iPadOS application switcher with the
// domain of the foreground tab and the tab count. Assumes the scene is
// visible. Will return nil if there are no tabs.
- (NSString*)displayTitleForAppSwitcher {
  if (tests_hook::ShouldLoadMinimalAppUI()) {
    return nil;
  }
  Browser* browser = self.currentInterface.browser;
  DCHECK(browser);

  if (self.profile->IsOffTheRecord()) {
    return nil;
  }
  web::WebState* webState = browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    return nil;
  }

  // At this point there is at least one tab.
  int numberOfTabs = browser->GetWebStateList()->count();
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

// Returns YES if the fullscreen sign-in promo should be presented.
- (BOOL)shouldPresentFullscreenSigninPromo {
  if (![self.mainCoordinator isTabAvailableToPresentViewController]) {
    return NO;
  }
  if (!signin::ShouldPresentUserSigninUpgrade(self.profile,
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
  if (self.sceneState.profileState.appState
          .fullscreenSigninPromoPresentedOnce) {
    return NO;
  }
  return YES;
}

// Presents the fullscreen sign-in  promo.
- (void)tryPresentFullscreenSigninPromo {
  // It is possible during a slow asynchronous call that the user changes their
  // state so as to no longer be eligible for sign-in promos. Return early in
  // this case.
  if (![self shouldPresentFullscreenSigninPromo]) {
    return;
  }
  self.sceneState.profileState.appState.fullscreenSigninPromoPresentedOnce =
      YES;
  id<SceneCommands> sceneHandler = HandlerForProtocol(
      self.mainInterface.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler showFullscreenSigninPromoWithCompletion:nil];
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
    if (self.mainCoordinator.isSigninInProgress) {
      // Return NO because intents cannot be handled when a sign-in is in
      // progress.
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

  if (tests_hook::ShouldLoadMinimalAppUI()) {
    return NO;
  }

  return YES;
}

- (BOOL)isSignedIn {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);
  CHECK(identityManager);

  return identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

- (void)handleModalsDismissalWithMode:(ApplicationModeForTabOpening)targetMode
                        urlLoadParams:(const UrlLoadParams&)urlLoadParams
                           completion:(ProceduralBlock)completion {
  // Disconnected scenes should no-op, since browser objects may not exist.
  if (self.sceneState.activationLevel == SceneActivationLevelDisconnected) {
    return;
  }

  if (!self.mainInterface || !self.mainInterface.browser) {
    return;
  }

  BOOL incognitoDisabled = [self isIncognitoDisabled];

  if ([self canShowIncognitoInterstitialForTargetMode:targetMode]) {
    [self.mainCoordinator
        showIncognitoInterstitialWithUrlLoadParams:urlLoadParams];
    completion();
  } else {
    if (incognitoDisabled &&
        targetMode == ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO) {
      UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
      [self openSelectedTabInMode:ApplicationModeForTabOpening::NORMAL
                withUrlLoadParams:params
                       completion:completion];
      [self.mainCoordinator
          showYoutubeIncognitoWithUrlLoadParams:urlLoadParams];
    } else {
      ApplicationModeForTabOpening tabOpeningMode =
          (targetMode == ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO)
              ? ApplicationModeForTabOpening::INCOGNITO
              : targetMode;
      [self openSelectedTabInMode:tabOpeningMode
                withUrlLoadParams:urlLoadParams
                       completion:completion];
      if (targetMode == ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO) {
        [self.mainCoordinator
            showYoutubeIncognitoWithUrlLoadParams:urlLoadParams];
      }
    }
  }
}

- (void)handleModelsDismissalWithReauthAgent:
            (IncognitoReauthSceneAgent*)reauthAgent
                     dismissModalsCompletion:
                         (ProceduralBlock)dismissModalsCompletion
                                  completion:(ProceduralBlock)completion {
  [reauthAgent authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
    if (success) {
      dismissModalsCompletion();
    } else {
      // Do not open the tab, but still call completion.
      if (completion) {
        completion();
      }
    }
  }];
}

// Add scene agents that are not dependent on profileState.
- (void)addAgents {
  SceneState* sceneState = self.sceneState;
  ProfileIOS* profile = self.profile;
  Browser* mainBrowser = self.browserLifecycleManager.mainInterface.browser;

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
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(mainCommandDispatcher, SceneCommands);
  id<PolicyChangeCommands> policyChangeCommandsHandler =
      HandlerForProtocol(mainCommandDispatcher, PolicyChangeCommands);

  [sceneState
      addAgent:[[SigninPolicySceneAgent alloc]
                       initWithSceneUIProvider:self
                                  sceneHandler:sceneHandler
                   policyChangeCommandsHandler:policyChangeCommandsHandler]];

  enterprise_idle::IdleService* idleService =
      enterprise_idle::IdleServiceFactory::GetForProfile(profile);
  id<SnackbarCommands> snackbarCommandsHandler =
      static_cast<id<SnackbarCommands>>(mainCommandDispatcher);

  [sceneState addAgent:[[IdleTimeoutPolicySceneAgent alloc]
                           initWithSceneUIProvider:self
                                      sceneHandler:sceneHandler
                           snackbarCommandsHandler:snackbarCommandsHandler
                                       idleService:idleService
                                       mainBrowser:mainBrowser]];

  // Now that the main browser's command dispatcher is created and the newly
  // started UI coordinators have registered with it, inject it into the
  // PolicyWatcherBrowserAgent so it can start monitoring UI-impacting policy
  // changes.
  PolicyWatcherBrowserAgent* policyWatcherAgent =
      PolicyWatcherBrowserAgent::FromBrowser(self.mainInterface.browser);
  policyWatcherAgent->Initialize(policyChangeCommandsHandler);

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

  if (IsDockingPromoV2Enabled()) {
    [sceneState addAgent:[[DockingPromoSceneAgent alloc]
                             initWithPromosManager:promosManager]];
  }

  if (IsDefaultBrowserPictureInPictureEnabled()) {
    [sceneState addAgent:[[PictureInPictureSceneAgent alloc] init]];
  }

  // Do not gate by feature flag so it can run for enabled -> disabled
  // scenarios.
  [sceneState addAgent:[[CredentialProviderPromoSceneAgent alloc]
                           initWithPromosManager:promosManager]];

  if (IsFullscreenSigninPromoManagerMigrationEnabled()) {
    AuthenticationService* authService =
        AuthenticationServiceFactory::GetForProfile(profile);
    PrefService* prefService = profile->GetPrefs();
    [sceneState
        addAgent:
            [[FullscreenSigninPromoSceneAgent alloc]
                initWithPromosManager:promosManager
                          authService:authService
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          profile)
                          syncService:SyncServiceFactory::GetForProfile(profile)
                          prefService:prefService]];
  }

  if (IsPageActionMenuEnabled()) {
    [sceneState addAgent:[[GeminiPromoSceneAgent alloc]
                             initWithPromosManager:promosManager]];
  }
}

// Adds agents that may depend on profileState. Called after a profileState has
// been connected to the sceneState.
- (void)addProfileStateDependentAgents {
  if (tests_hook::ShouldLoadMinimalAppUI()) {
    return;
  }

  [_sceneState addAgent:[[UIBlockerSceneAgent alloc] init]];
  [_sceneState addAgent:[[IncognitoBlockerSceneAgent alloc] init]];
  [_sceneState
      addAgent:[[IncognitoReauthSceneAgent alloc]
                   initWithReauthModule:[[ReauthenticationModule alloc] init]
                           sceneHandler:_mainCoordinator]];
  [_sceneState addAgent:[[StartSurfaceSceneAgent alloc] init]];
  [_sceneState addAgent:[[SessionSavingSceneAgent alloc] init]];
  [_sceneState addAgent:[[LayoutGuideSceneAgent alloc] init]];
  [_sceneState addAgent:[[ShareExtensionSceneAgent alloc] init]];

  if (IsEnableNewStartupFlowEnabled()) {
    [_sceneState addAgent:[[TaskUpdaterSceneAgent alloc] init]];
  }
}

// Dismisses modal dialogs via the scene handler and optionally dismisses the
// omnibox.
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  id<SceneCommands> sceneHandler = HandlerForProtocol(
      self.currentBrowserForURLLoading->GetCommandDispatcher(), SceneCommands);
  [sceneHandler dismissModalDialogsWithCompletion:completion
                                   dismissOmnibox:dismissOmnibox
                                 dismissSnackbars:YES];
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
    case START_VOICE_SEARCH: {
      __weak id<BrowserCoordinatorCommands> weakHandler = HandlerForProtocol(
          self.currentInterface.browser->GetCommandDispatcher(),
          BrowserCoordinatorCommands);
      return ^{
        [weakHandler startVoiceSearch];
      };
    }
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
    case VIEW_HISTORY: {
      __weak id<SceneCommands> weakSceneHandler = HandlerForProtocol(
          self.currentInterface.browser->GetCommandDispatcher(), SceneCommands);
      return ^{
        [weakSceneHandler showHistory];
      };
    }
    case OPEN_PAYMENT_METHODS:
      return ^{
        [weakSelf openPaymentMethods];
      };
    case RUN_SAFETY_CHECK: {
      __weak id<SettingsCommands> weakSettingsHandler = HandlerForProtocol(
          self.currentInterface.browser->GetCommandDispatcher(),
          SettingsCommands);
      return ^{
        [weakSettingsHandler showAndStartSafetyCheckForReferrer:
                                 password_manager::PasswordCheckReferrer::
                                     kSafetyCheckMagicStack];
      };
    }
    case MANAGE_PASSWORDS: {
      __weak id<SettingsCommands> weakSettingsHandler = HandlerForProtocol(
          self.currentInterface.browser->GetCommandDispatcher(),
          SettingsCommands);
      return ^{
        [weakSettingsHandler showPasswordSearchPage];
      };
    }
    case MANAGE_SETTINGS: {
      __weak id<SceneCommands> weakSceneHandler = HandlerForProtocol(
          self.currentInterface.browser->GetCommandDispatcher(), SceneCommands);
      return ^{
        [weakSceneHandler
            showSettingsFromViewController:weakSelf.currentInterface
                                               .viewController];
      };
    }
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
    case START_LENS_FROM_SHARE_EXTENSION:
      return ^{
        [weakSelf searchShareExtensionImageWithLens];
      };
    case CREDENTIAL_EXCHANGE_IMPORT:
      if (@available(iOS 26, *)) {
        return ^{
          [weakSelf importCredentials];
        };
      } else {
        NOTREACHED() << "Credential import is available on iOS 26+ only.";
      }
    default:
      return nil;
  }
}

// Starts a lens search for share extension.
- (void)searchShareExtensionImageWithLens {
  CHECK(_imageSearchData);
  if (!_imageTranscoder) {
    _imageTranscoder = std::make_unique<web::JavaScriptImageTranscoder>();
  }
  __weak __typeof(self) weakSelf = self;

  _imageTranscoder->TranscodeImage(
      _imageSearchData, @"image/jpeg", nil, nil, nil,
      base::BindOnce(
          [](SceneController* controller, NSData* safeImageData,
             NSError* error) {
            [controller triggerLensSearchWithImageData:safeImageData
                                                 error:error];
          },
          weakSelf));
}

// Triggers a lens seach based on a given trusted image data.
- (void)triggerLensSearchWithImageData:(NSData*)imageData
                                 error:(NSError*)error {
  if (error) {
    return;
  }

  id<LensCommands> lensHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), LensCommands);
  UIImage* image = [UIImage imageWithData:imageData];
  SearchImageWithLensCommand* command = [[SearchImageWithLensCommand alloc]
      initWithImage:image
         // TODO(crbug.com/403235333): Add Lens entry point for Share extension.
         entryPoint:LensEntrypoint::ContextMenu];
  [lensHandler searchImageWithLens:command];
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
  id<BrowserCoordinatorCommands> browserCoordinatorHandler =
      HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                         BrowserCoordinatorCommands);
  [browserCoordinatorHandler showComposebox];
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
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
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
  if (!self.mainInterface.browser || [self isIncognitoForced]) {
    return;
  }

  __weak CommandDispatcher* weakDispatcher =
      self.mainInterface.browser->GetCommandDispatcher();
  ProceduralBlock openQuickDeleteBlock = ^{
    id<QuickDeleteCommands> quickDeleteHandler =
        HandlerForProtocol(weakDispatcher, QuickDeleteCommands);
    [quickDeleteHandler showQuickDeleteAndCanPerformRadialWipeAnimation:YES];
  };

  if (self.currentInterface.incognito) {
    [self.mainCoordinator openNonIncognitoTab:openQuickDeleteBlock];
  } else {
    openQuickDeleteBlock();
  }
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

  [bookmarksCommandsHandler addBookmarks:URLs];
}

- (void)addReadingListItems:(NSArray<NSURL*>*)URLs {
  if (!self.currentInterface.browser || [URLs count] < 1) {
    return;
  }

  ReadingListBrowserAgent* readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(self.currentInterface.browser);

  readingListBrowserAgent->BulkAddURLsToReadingListWithViewSnackbar(URLs);
}

- (void)importCredentials API_AVAILABLE(ios(26.0)) {
  id<SettingsCommands> settingsHandler = HandlerForProtocol(
      self.currentInterface.browser->GetCommandDispatcher(), SettingsCommands);
  [settingsHandler
      showPasswordManagerForCredentialImport:self.startupParameters
                                                 .credentialExchangeImportUUID];
}

#pragma mark - TabOpening implementation.

- (void)dismissModalsAndMaybeOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                                 withUrlLoadParams:
                                     (const UrlLoadParams&)urlLoadParams
                                    dismissOmnibox:(BOOL)dismissOmnibox
                                        completion:(ProceduralBlock)completion {
  PrefService* prefs = GetApplicationContext()->GetLocalState();
  BOOL canShowIncognitoInterstitial =
      prefs->GetBoolean(prefs::kIncognitoInterstitialEnabled);

  if ([self isIncognitoForced]) {
    targetMode = ApplicationModeForTabOpening::INCOGNITO;
  } else if ([self isIncognitoDisabled]) {
    targetMode = ApplicationModeForTabOpening::NORMAL;
  } else if (!canShowIncognitoInterstitial &&
             targetMode == ApplicationModeForTabOpening::UNDETERMINED) {
    // Fallback to NORMAL mode if the Incognito interstitial is not
    // available.
    targetMode = ApplicationModeForTabOpening::NORMAL;
  }

  UrlLoadParams copyOfUrlLoadParams = urlLoadParams;

  __weak SceneController* weakSelf = self;
  void (^dismissModalsCompletion)() = ^{
    [weakSelf handleModalsDismissalWithMode:targetMode
                              urlLoadParams:copyOfUrlLoadParams
                                 completion:completion];
  };

  if (targetMode == ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO) {
    targetMode = ApplicationModeForTabOpening::INCOGNITO;
  }

  // Wrap the post-dismiss-modals action with the incognito auth check.
  if (targetMode == ApplicationModeForTabOpening::INCOGNITO) {
    IncognitoReauthSceneAgent* reauthAgent =
        [IncognitoReauthSceneAgent agentFromScene:self.sceneState];
    if (reauthAgent.authenticationRequired) {
      void (^wrappedDismissModalCompletion)() = dismissModalsCompletion;
      dismissModalsCompletion = ^{
        [weakSelf
            handleModelsDismissalWithReauthAgent:reauthAgent
                         dismissModalsCompletion:wrappedDismissModalCompletion
                                      completion:completion];
      };
    }
  }

  // TODO(crbug.com/471130344): This branch addresses a privacy leak where
  // Composebox UI interactions were exposed from incognito to non-incognito
  // sessions.
  //
  // While the legacy Omnibox UI was also affected by this bug, it was
  // incidentally dismissed because it was presented as a BVC subview.
  // Since the Composebox is presented independently on top of the view stack,
  // the dismissal logic must be explicitly addressed here.
  //
  // Refer to crbug.com/470968439 for additional context.
  if (IsComposeboxIOSEnabled()) {
    BOOL alreadyInIncognito = self.currentInterface.profile->IsOffTheRecord();
    BOOL targetModeIncognito =
        targetMode == ApplicationModeForTabOpening::INCOGNITO;
    // The composebox UI and its dependencies are browser-scoped and
    // instantiated upon creation.
    //
    // When the context changes (e.g., transitioning to Incognito mode), any
    // preexisting Composebox UI must be dismissed and recreated to ensure its
    // underlying dependencies remain synchronized with the new environment.
    if (alreadyInIncognito != targetModeIncognito) {
      dismissOmnibox = YES;
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

  if (IsComposeboxIOSEnabled()) {
    BOOL alreadyInIncognito = self.currentInterface.profile->IsOffTheRecord();
    // The composebox UI and its dependencies are browser-scoped and
    // instantiated upon creation.
    //
    // When the context changes (e.g., transitioning to Incognito mode), any
    // preexisting Composebox UI must be dismissed and recreated to ensure its
    // underlying dependencies remain synchronized with the new environment.
    if (alreadyInIncognito != incognitoMode) {
      dismissOmnibox = YES;
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
  DCHECK(self.browserLifecycleManager);
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

  self.browserLifecycleManager.currentInterface = newInterface;

  if (!self.activatingBrowser) {
    [self displayCurrentBVCAndFocusOmnibox:NO];
  }

  // Tell the BVC that was made current that it can use the web.
  [self activateBVCAndMakeCurrentBVCPrimary];
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
  DCHECK(tabOpeningTargetMode !=
         ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO);
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
  _imageSearchData = [self.startupParameters imageSearchData];
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
    [self setCurrentInterfaceForMode:targetMode];
    [self openOrReuseTabInMode:targetMode
             withUrlLoadParams:urlLoadParams
           tabOpenedCompletion:tabOpenedCompletion];
  }
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

- (UrlLoadingBrowserAgent*)browserAgentForIncognito:(BOOL)incognito {
  if (incognito) {
    return UrlLoadingBrowserAgent::FromBrowser(self.incognitoInterface.browser);
  }
  return UrlLoadingBrowserAgent::FromBrowser(self.mainInterface.browser);
}

// Asks the respective Snapshot helper to update the snapshot for the active
// WebState.
- (void)updateActiveWebStateSnapshot {
  // During startup, there may be no current interface. Do nothing in that
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

// Displays current (incognito/normal) BVC and optionally focuses the omnibox.
- (void)displayCurrentBVCAndFocusOmnibox:(BOOL)focusOmnibox {
  ProceduralBlock completion = nil;
  if (focusOmnibox) {
    id<BrowserCoordinatorCommands> browserCoordinatorHandler =
        HandlerForProtocol(
            self.currentInterface.browser->GetCommandDispatcher(),
            BrowserCoordinatorCommands);
    completion = ^{
      [browserCoordinatorHandler showComposebox];
    };
  }
  [self displayCurrentBVC:completion];
}

#pragma mark - SceneUIHandler

- (void)displayCurrentBVC:(ProceduralBlock)completion {
  [self.mainCoordinator
      showBrowserLayoutViewController:self.currentInterface
                                          .browserLayoutViewController
                            incognito:self.currentInterface.incognito
                           completion:completion];
  [HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                      SceneCommands)
      setIncognitoContentVisible:self.currentInterface.incognito];
}

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

  // Refrain from reusing the same tab for Lens Overlay initiated requests.
  BOOL initiatedByLensOverlay = false;
  if (currentWebState) {
    if (LensOverlayTabHelper* lensOverlayTabHelper =
            LensOverlayTabHelper::FromWebState(currentWebState)) {
      initiatedByLensOverlay =
          lensOverlayTabHelper->IsLensOverlayUIAttachedAndAlive();
    }
  }

  BOOL forceNewTabForIntentSearch =
      base::FeatureList::IsEnabled(kForceNewTabForIntentSearch) &&
      (self.startupParameters.postOpeningAction == FOCUS_OMNIBOX);
  BOOL alwaysInsertNewTab =
      initiatedByLensOverlay || forceNewTabForIntentSearch;

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

  BOOL isSharedTabGroupJoinURL =
      data_sharing::DataSharingUtils::ShouldInterceptNavigationForShareURL(
          urlLoadParams.web_params.url);

  CHECK(!(isSharedTabGroupJoinURL && alwaysInsertNewTab));

  // If the current tab isn't an NTP, open a new tab.  Be sure to use
  // -GetLastCommittedURL incase the NTP is still loading.
  BOOL shouldOpenNewTab =
      alwaysInsertNewTab ||
      !(currentWebState && IsUrlNtp(currentWebState->GetVisibleURL()));

  if (isSharedTabGroupJoinURL) {
    // If it is a URL to join a tab group, it should be opened in the current
    // tab as the load will be canceled.
    shouldOpenNewTab = NO;
  }

  if (shouldOpenNewTab) {
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

// Returns the condition to check in order to show the `IncognitoIntertitial`
// for a given `ApplicationModeForTabOpening`.
- (BOOL)canShowIncognitoInterstitialForTargetMode:
    (ApplicationModeForTabOpening)targetMode {
  // The incognito intertitial can be shown in two cases:
  //    1- The incognito interstitial is enabled and the target mode is either
  //    `UNDETERMINED` or `APP_SWITCHER_INCOGNITO`.
  //    2- The mode is `APP_SWITCHER_UNDETERMINED`.
  PrefService* prefs = GetApplicationContext()->GetLocalState();
  BOOL shouldShowIncognitoInterstitial =
      prefs->GetBoolean(prefs::kIncognitoInterstitialEnabled) &&
      (targetMode == ApplicationModeForTabOpening::UNDETERMINED ||
       targetMode == ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO);
  return shouldShowIncognitoInterstitial ||
         targetMode == ApplicationModeForTabOpening::APP_SWITCHER_UNDETERMINED;
}

#pragma mark - Helpers for web state list events

// Called when the last incognito tab was closed.
- (void)lastIncognitoTabClosed {
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
           startupInformation:self.sceneState.profileState.startupInformation
                  prefService:self.currentInterface.profile->GetPrefs()
                    initStage:self.sceneState.profileState.initStage];
  }
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

- (void)willDestroyIncognitoProfile {
  // Clear the Incognito Browser and notify the TabGrid that its otrBrowser
  // will be destroyed.
  self.mainCoordinator.incognitoBrowser = nil;

  if (breadcrumbs::IsEnabled(GetApplicationContext()->GetLocalState())) {
    BreadcrumbManagerBrowserAgent::FromBrowser(self.incognitoInterface.browser)
        ->SetLoggingEnabled(false);
  }

  _incognitoWebStateObserver.reset();
  [self.browserLifecycleManager willDestroyIncognitoProfile];
}

- (void)incognitoProfileCreated {
  [self.browserLifecycleManager incognitoProfileCreated];

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

#pragma mark - SceneUIProvider

- (UIViewController*)activeViewController {
  return self.mainCoordinator.activeViewController;
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  [self signoutIfNeeded];
}

@end
