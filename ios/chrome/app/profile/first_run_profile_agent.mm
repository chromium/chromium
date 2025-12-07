// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/first_run_profile_agent.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/metrics/metrics_service.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/device_orientation/ui_bundled/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_post_action_provider.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_provider.h"
#import "ios/chrome/browser/first_run/ui_bundled/guided_tour/guided_tour_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/guided_tour/guided_tour_promo_coordinator.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_entry_point.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_ui_handler.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/commands/synced_set_up_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/synced_set_up/public/synced_set_up_metrics.h"
#import "ios/chrome/browser/synced_set_up/utils/utils.h"

namespace first_run {

// Helper class used to access the passkey needed to call
// MetricsService::StartOutOfBandUploadIfPossible().
class FirstRunProfileAgentMetricsHelper final {
 public:
  FirstRunProfileAgentMetricsHelper() {}
  ~FirstRunProfileAgentMetricsHelper() {}

  // Triggers an UMA metrics log upload.
  void StartOutOfBandUploadIfPossible() {
    metrics::MetricsService* metrics_service =
        GetApplicationContext()->GetMetricsService();
    // MetricsService can be nil for TestingApplicationContext.
    if (metrics_service) {
      metrics_service->StartOutOfBandUploadIfPossible(
          metrics::MetricsService::OutOfBandUploadPasskey());
    }
  }
};

}  // namespace first_run

namespace {

// Metrics logged for the Guided Tour.
const char kGuidedTourPromoResultHistogram[] = "IOS.GuidedTour.Promo.DidAccept";
const char kGuidedTourStepDidFinishHistogram[] = "IOS.GuidedTour.DidFinishStep";

}  // namespace

@interface FirstRunProfileAgent () <FirstRunCoordinatorDelegate,
                                    GuidedTourCoordinatorDelegate,
                                    GuidedTourPromoCoordinatorDelegate,
                                    SafariDataImportUIHandler,
                                    SceneStateObserver>

@end

@implementation FirstRunProfileAgent {
  // UI blocker used while the FRE UI is shown in the scene controlled by this
  // object.
  std::unique_ptr<ScopedUIBlocker> _firstRunUIBlocker;

  // The scene that is chosen for presenting the FRE on.
  SceneState* _presentingSceneState;

  // Coordinator of the First Run UI.
  FirstRunCoordinator* _firstRunCoordinator;

  // Provider for actions that should be completed after the first run screens.
  // Lazy loaded after the screens complete presentation.
  FirstRunPostActionProvider* _postActionsProvider;
  BOOL _postActionsCompleted;

  // Coordinator for the Guided Tour Promo.
  GuidedTourPromoCoordinator* _guidedTourPromoCoordinator;

  // Coordinator for the first step of the guided tour.
  GuidedTourCoordinator* _guidedTourCoordinator;

  // The current step in the guided tour.
  GuidedTourStep _currentGuidedTourStep;

  // Used to force the device orientation in portrait mode on iPhone.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;

  // Used to prevent other IPHs from showing during the Guided Tour.
  std::unique_ptr<feature_engagement::DisplayLockHandle> _displayLock;
}

#pragma mark - Public

- (void)tabGridWasPresented {
  if (_currentGuidedTourStep == GuidedTourStep::kTabGridIncognito) {
    id<TabGridToolbarCommands> handler =
        HandlerForProtocol([self commandDispatcher], TabGridToolbarCommands);
    __weak FirstRunProfileAgent* weakSelf = self;
    ProceduralBlock completion = ^{
      [weakSelf showLongPressStep];
    };
    [handler showGuidedTourIncognitoStepWithDismissalCompletion:completion];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  _firstRunUIBlocker.reset();

  [_firstRunCoordinator stop];
  _firstRunCoordinator = nil;

  [sceneState removeObserver:self];
  _presentingSceneState = nil;
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    willTransitionToInitStage:(ProfileInitStage)nextInitStage
                fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFirstRun) {
    return;
  }

  AppState* appState = profileState.appState;
  if (appState.startupInformation.isFirstRun) {
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
    if (IsBestOfAppFREEnabled()) {
      id<BrowserProvider> presentingInterface =
          _presentingSceneState.browserProviderInterface.currentBrowserProvider;
      Browser* browser = presentingInterface.browser;
      feature_engagement::Tracker* engagementTracker =
          feature_engagement::TrackerFactory::GetForProfile(
              browser->GetProfile()->GetOriginalProfile());
      _displayLock = engagementTracker->AcquireDisplayLock();
    }
  }
}

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFirstRun) {
    [self handleFirstRunStage];
    return;
  }

  if (fromInitStage == ProfileInitStage::kFirstRun && _postActionsCompleted) {
    [profileState removeAgent:self];
  }
}

- (void)profileState:(ProfileState*)profileState
    firstSceneHasInitializedUI:(SceneState*)sceneState {
  // Select the first scene that the app declares as initialized to present
  // the FRE UI on.
  _presentingSceneState = sceneState;
  [_presentingSceneState addObserver:self];

  if (self.profileState.initStage != ProfileInitStage::kFirstRun) {
    return;
  }

  // Skip the FRE because it wasn't determined to be needed.
  if (!self.profileState.appState.startupInformation.isFirstRun) {
    return;
  }

  [self showFirstRunUI];
}

#pragma mark - GuidedTourCoordinatorDelegate

- (void)stepCompleted:(GuidedTourStep)step {
  CHECK_EQ(step, _currentGuidedTourStep);
  if (step == GuidedTourStep::kNTP) {
    _currentGuidedTourStep = GuidedTourStep::kTabGridIncognito;
    id<ApplicationCommands> applicationHandler =
        HandlerForProtocol([self commandDispatcher], ApplicationCommands);
    [applicationHandler displayTabGridInMode:TabGridOpeningMode::kRegular];
  }
}

- (void)nextTappedForStep:(GuidedTourStep)step {
  base::UmaHistogramEnumeration(kGuidedTourStepDidFinishHistogram, step);
  if (IsManualUploadForBestOfAppEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          first_run::FirstRunProfileAgentMetricsHelper metricsHelper;
          metricsHelper.StartOutOfBandUploadIfPossible();
        }));
  }
  if (step == GuidedTourStep::kNTP) {
    id<GuidedTourCommands> handler =
        HandlerForProtocol([self commandDispatcher], GuidedTourCommands);
    [handler stepCompleted:GuidedTourStep::kNTP];
  }
}

#pragma mark - GuidedTourPromoCoordinatorDelegate

- (void)dismissGuidedTourPromo {
  [_guidedTourPromoCoordinator stopWithCompletion:nil];
  [self guidedTourCompleted];
  [self logGuidedTourPromoResult:NO];
}

- (void)startGuidedTour {
  __weak FirstRunProfileAgent* weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf showNTPStep];
  };
  [_guidedTourPromoCoordinator stopWithCompletion:completion];
  [self logGuidedTourPromoResult:YES];
}

#pragma mark - FirstRunCoordinatorDelegate

- (void)didFinishFirstRun {
  DCHECK_EQ(self.profileState.initStage, ProfileInitStage::kFirstRun);
  __weak FirstRunProfileAgent* weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf performNextPostFirstRunAction];
  };
  [_firstRunCoordinator stopWithCompletion:completion];
  _firstRunCoordinator = nil;
  [self.profileState queueTransitionToNextInitStage];
}

#pragma mark - SafariDataImportUIHandler

- (void)safariDataImportDidDismiss {
  [self performNextPostFirstRunAction];
}

#pragma mark - Private

// Command dispatcher for the presenting scene state.
- (CommandDispatcher*)commandDispatcher {
  id<BrowserProvider> presentingInterface =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider;
  Browser* browser = presentingInterface.browser;
  return browser->GetCommandDispatcher();
}

// Returns the original (i.e., not off-the-record) profile associated with the
// current browser. May return nullptr.
- (ProfileIOS*)originalProfile {
  id<BrowserProvider> presentingInterface =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider;
  Browser* browser = presentingInterface.browser;
  if (!browser || !browser->GetProfile()) {
    return nullptr;
  }
  return browser->GetProfile()->GetOriginalProfile();
}

// Handles the First Run stage of app startup.
- (void)handleFirstRunStage {
  // Skip the FRE because it wasn't determined to be needed.
  if (!self.profileState.appState.startupInformation.isFirstRun) {
    _postActionsCompleted = YES;
    [self releaseUILocks];
    [self.profileState queueTransitionToNextInitStage];
    return;
  }

  // Cannot show the FRE UI immediately because there is no scene state to
  // present from.
  if (!_presentingSceneState) {
    return;
  }

  [self showFirstRunUI];
}

// Starts the First Run Experience flow.
- (void)showFirstRunUI {
  DCHECK_EQ(self.profileState.initStage, ProfileInitStage::kFirstRun);

  // There must be a designated presenting scene before showing the first run
  // UI.
  DCHECK(_presentingSceneState);

  ProfileIOS* profile = [self originalProfile];

  DCHECK(!_firstRunUIBlocker);
  _firstRunUIBlocker = std::make_unique<ScopedUIBlocker>(_presentingSceneState);

  // TODO(crbug.com/343699504): Remove pre-fetching capabilities once these are
  // loaded in iSL.
  RunSystemCapabilitiesPrefetch(signin::GetIdentitiesOnDevice(profile));

  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile];
  UIViewController* baseViewController =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider
          .viewController;
  Browser* mainBrowser = _presentingSceneState.browserProviderInterface
                             .mainBrowserProvider.browser;
  _firstRunCoordinator =
      [[FirstRunCoordinator alloc] initWithBaseViewController:baseViewController
                                                      browser:mainBrowser
                                               screenProvider:provider];
  _firstRunCoordinator.delegate = self;
  [_firstRunCoordinator start];
}

// Helper method that performs actions sequentially after the FRE screens are
// finished presenting.
- (void)performNextPostFirstRunAction {
  if (!_postActionsProvider) {
    PrefService* prefService = [self profilePrefs];
    _postActionsProvider =
        [[FirstRunPostActionProvider alloc] initWithPrefService:prefService];
  }
  switch ([_postActionsProvider nextScreenType]) {
    case kSyncedSetUp:
      [self showSyncedSetUp];
      break;
    case kGuidedTour:
      [self showGuidedTourPrompt];
      break;
    case kSafariImport:
      [self displaySafariDataImportEntryPoint];
      break;
    case kStepsCompleted:
      _postActionsCompleted = YES;
      [self releaseUILocks];
      if (self.profileState.initStage >= ProfileInitStage::kFirstRun) {
        [self.profileState removeAgent:self];
      }
      break;
    case kSignIn:
    case kHistorySync:
    case kDefaultBrowserPromo:
    case kChoice:
    case kDockingPromo:
    case kBestFeatures:
    case kLensInteractivePromo:
    case kLensAnimatedPromo:
    default:
      NOTREACHED() << "Not a post first run action.";
  }
}

// Shows the initial prompt for the Guided Tour promo.
- (void)showGuidedTourPrompt {
  if (_guidedTourPromoCoordinator) {
    return;
  }
  id<BrowserProvider> presentingInterface =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider;
  Browser* browser = presentingInterface.browser;
  _guidedTourPromoCoordinator = [[GuidedTourPromoCoordinator alloc]
      initWithBaseViewController:presentingInterface.viewController
                         browser:browser];
  _guidedTourPromoCoordinator.delegate = self;
  [_guidedTourPromoCoordinator start];
}

// Shows the New Tab Page step of the Guided Tour.
- (void)showNTPStep {
  _currentGuidedTourStep = GuidedTourStep::kNTP;
  id<GuidedTourCommands> handler =
      HandlerForProtocol([self commandDispatcher], GuidedTourCommands);
  [handler highlightViewInStep:GuidedTourStep::kNTP];

  id<BrowserProvider> presentingInterface =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider;
  _guidedTourCoordinator = [[GuidedTourCoordinator alloc]
            initWithStep:GuidedTourStep::kNTP
      baseViewController:presentingInterface.viewController
                 browser:presentingInterface.browser
                delegate:self];
  [_guidedTourCoordinator start];
}

// Shows the Long Press step of the Guided Tour in the tab grid..
- (void)showLongPressStep {
  _currentGuidedTourStep = GuidedTourStep::kTabGridLongPress;
  __weak FirstRunProfileAgent* weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf showTabGroupStep];
  };
  id<TabGridCommands> handler =
      HandlerForProtocol([self commandDispatcher], TabGridCommands);
  [handler showGuidedTourLongPressStepWithDismissalCompletion:completion];
}

// Shows the Tab Group step of the Guided Tour in the tab grid.
- (void)showTabGroupStep {
  _currentGuidedTourStep = GuidedTourStep::kTabGridTabGroup;
  id<TabGridToolbarCommands> handler =
      HandlerForProtocol([self commandDispatcher], TabGridToolbarCommands);
  __weak FirstRunProfileAgent* weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf guidedTourCompleted];
  };
  [handler showGuidedTourTabGroupStepWithDismissalCompletion:completion];
}

// Called when the Guided Tour flow is completed.
- (void)guidedTourCompleted {
  [self releaseUILocks];
  [self performNextPostFirstRunAction];
}

// Shows the entry point to import data from Safari.
- (void)displaySafariDataImportEntryPoint {
  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol([self commandDispatcher], ApplicationCommands);
  [applicationHandler displaySafariDataImportFromEntryPoint:
                          SafariDataImportEntryPoint::kFirstRun
                                              withUIHandler:self];
}

// Logs the user decision for the Guided Tour promo.
- (void)logGuidedTourPromoResult:(BOOL)didAccept {
  base::UmaHistogramBoolean(kGuidedTourPromoResultHistogram, didAccept);
  if (IsManualUploadForBestOfAppEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          first_run::FirstRunProfileAgentMetricsHelper metricsHelper;
          metricsHelper.StartOutOfBandUploadIfPossible();
        }));
  }
}

// Release UI locks that prohibits orientation change, more IPHs and overlays.
- (void)releaseUILocks {
  _displayLock.reset();
  _scopedForceOrientation.reset();
  _firstRunUIBlocker.reset();
}

// Returns the profile pref service for the original (i.e., not off-the-record)
// profile associated with the current browser. May return nullptr.
- (PrefService*)profilePrefs {
  ProfileIOS* profile = [self originalProfile];
  return profile ? profile->GetPrefs() : nullptr;
}

// Starts the Synced Set Up screen.
- (void)showSyncedSetUp {
  PrefService* profilePrefService = [self profilePrefs];

  if (!CanShowSyncedSetUp(profilePrefService)) {
    [self performNextPostFirstRunAction];
    return;
  }

  LogSyncedSetUpTriggerSource(SyncedSetUpTriggerSource::kPostFirstRun);

  __weak __typeof(self) weakSelf = self;

  id<SyncedSetUpCommands> syncedSetUpCommandsHandler =
      HandlerForProtocol([self commandDispatcher], SyncedSetUpCommands);
  [syncedSetUpCommandsHandler showSyncedSetUpWithDismissalCompletion:^{
    [weakSelf performNextPostFirstRunAction];
  }];
}

@end
