// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client_delegate.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator_delegate.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_detents_manager.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_metrics_recorder.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_pan_tracker.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/model/snapshot_cover_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_network_issue_alert_presenter.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_consumer.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/omnibox_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/omnibox/chrome_omnibox_client_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "ios/chrome/browser/web/model/web_state_delegate_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

LensEntrypoint LensEntrypointFromOverlayEntrypoint(
    LensOverlayEntrypoint overlayEntrypoint) {
  switch (overlayEntrypoint) {
    case LensOverlayEntrypoint::kLocationBar:
      return LensEntrypoint::LensOverlayLocationBar;
    case LensOverlayEntrypoint::kOverflowMenu:
      return LensEntrypoint::LensOverlayOverflowMenu;
  }
}

const CGFloat kSelectionOffsetPadding = 100.0f;

// The maximum height of the bottom sheet before it automatically closes when
// released.
const CGFloat kThresholdHeightForClosingSheet = 200.0f;

// The expected number of animations happening at the same time when exiting.
const int kExpectedExitAnimationCount = 2;

// The duration of the dismiss animation when exiting the selection UI.
const CGFloat kSelectionViewDismissAnimationDuration = 0.2f;

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
const CGFloat kMenuSymbolSize = 18;
#endif

}  // namespace

@interface LensOverlayCoordinator () <LensOverlayCommands,
                                      LensOverlayMediatorDelegate,
                                      LensOverlayResultConsumer,
                                      LensOverlayDetentsChangeObserver,
                                      LensOverlayConsentViewControllerDelegate,
                                      LensOverlayPanTrackerDelegate,
                                      LensOverlayNetworkIssueDelegate>

// Whether the `_containerViewController` is currently presented.
@property(nonatomic, assign, readonly) BOOL isLensOverlayVisible;

// Whether the UI is created.
@property(nonatomic, assign, readonly) BOOL isUICreated;

@end

@implementation LensOverlayCoordinator {
  /// Container view controller.
  /// Hosts all of lens UI: contains the selection UI, presents the results UI
  /// modally.
  LensOverlayContainerViewController* _containerViewController;

  /// The mediator for lens overlay.
  LensOverlayMediator* _mediator;

  /// The view controller for lens results.
  LensResultPageViewController* _resultViewController;
  /// The mediator for lens results.
  LensResultPageMediator* _resultMediator;
  /// The context menu configuration provider for the result page.
  ContextMenuConfigurationProvider* _resultContextMenuProvider;

  /// The tab helper associated with the current UI.
  raw_ptr<LensOverlayTabHelper> _associatedTabHelper;

  /// Coordinator of the omnibox.
  OmniboxCoordinator* _omniboxCoordinator;

  LensOverlayConsentViewController* _consentViewController;

  UIViewController<ChromeLensOverlay>* _selectionViewController;

  /// Indicates the Lens Overlay is in the exit flow.
  BOOL _isExiting;
  /// Forces the device orientation in portrait mode.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;
  /// Tracks whether the user is currently touching the screen.
  LensOverlayPanTracker* _windowPanTracker;
  /// Used to monitor the results sheet position relative to the container.
  CADisplayLink* _displayLink;
  /// Command handler for loadQueryCommands.
  id<LoadQueryCommands> _loadQueryHandler;

  /// Orchestrates the change in detents of the associated bottom sheet.
  LensOverlayDetentsManager* _detentsManager;

  /// This auxiliary window is used while restoring the sheet state when
  /// returning to the tab where Lens Overlay is active.
  UIWindow* _restorationWindow;

  /// A helper object that provides a central point for recording metrics.
  LensOverlayMetricsRecorder* _metricsRecorder;

  /// Network issue alert presenter.
  LensOverlayNetworkIssueAlertPresenter* _networkIssueAlertPresenter;
}

#pragma mark - public

- (UIViewController*)viewController {
  return _containerViewController;
}

#pragma mark - Helpers

// Returns whether the UI was created succesfully.
- (BOOL)createUIWithSnapshot:(UIImage*)snapshot
                  entrypoint:(LensOverlayEntrypoint)entrypoint {
  [self createContainerViewController];

  [self createSelectionViewControllerWithSnapshot:snapshot
                                       entrypoint:entrypoint];
  if (!_selectionViewController) {
    return NO;
  }

  [self createMediator];

  // Wire up consumers and delegates
  _containerViewController.selectionViewController = _selectionViewController;
  [_selectionViewController setLensOverlayDelegate:_mediator];
  _mediator.lensHandler = _selectionViewController;
  _mediator.commandsHandler = self;
  _mediator.delegate = self;
  // The mediator might destroy lens UI if the search engine doesn't support
  // lens.
  _mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.browser->GetProfile());

  _networkIssueAlertPresenter = [[LensOverlayNetworkIssueAlertPresenter alloc]
      initWithBaseViewController:_containerViewController];
  _networkIssueAlertPresenter.delegate = self;

  if ([self termsOfServiceAccepted]) {
    [_selectionViewController start];
  }
  return YES;
}

- (void)createSelectionViewControllerWithSnapshot:(UIImage*)snapshot
                                       entrypoint:
                                           (LensOverlayEntrypoint)entrypoint {
  if (_selectionViewController) {
    return;
  }
  LensConfiguration* config =
      [self createLensConfigurationForEntrypoint:entrypoint];
  NSArray<UIAction*>* additionalMenuItems = @[
    [self openUserActivityAction],
    [self learnMoreAction],
  ];

  _selectionViewController = ios::provider::NewChromeLensOverlay(
      snapshot, config, additionalMenuItems);
}

- (void)createContainerViewController {
  if (_containerViewController) {
    return;
  }
  _containerViewController = [[LensOverlayContainerViewController alloc]
      initWithLensOverlayCommandsHandler:self];
  _containerViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  _containerViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
}

- (void)createMediator {
  if (_mediator) {
    return;
  }
  Browser* browser = self.browser;
  _mediator = [[LensOverlayMediator alloc]
      initWithIsIncognito:browser->GetProfile()->IsOffTheRecord()];
  _mediator.applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);

  // Results UI is lazily initialized; see comment in LensOverlayResultConsumer
  // section.
  _mediator.resultConsumer = self;
}

- (BOOL)createConsentViewController {
  _consentViewController = [[LensOverlayConsentViewController alloc] init];
  _consentViewController.delegate = self;

  return YES;
}

#pragma mark - ChromeCoordinator

- (void)start {
  CHECK(IsLensOverlayAvailable());
  [super start];

  Browser* browser = self.browser;
  CHECK(browser, kLensOverlayNotFatalUntil);

  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensOverlayCommands)];
  _loadQueryHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), LoadQueryCommands);
}

- (void)stop {
  if (Browser* browser = self.browser) {
    [browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  }
  [self destroyLensUI:NO reason:lens::LensOverlayDismissalSource::kTabClosed];

  [super stop];
}

#pragma mark - LensOverlayCommands

- (void)createAndShowLensUI:(BOOL)animated
                 entrypoint:(LensOverlayEntrypoint)entrypoint
                 completion:(void (^)(BOOL))completion {
  if (self.isUICreated) {
    // The UI is probably associated with the non-active tab. Destroy it with no
    // animation.
    [self destroyLensUI:NO
                 reason:lens::LensOverlayDismissalSource::kNewLensInvocation];
  }

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(lowMemoryWarningReceived)
             name:UIApplicationDidReceiveMemoryWarningNotification
           object:nil];

  _associatedTabHelper = [self activeTabHelper];
  CHECK(_associatedTabHelper, kLensOverlayNotFatalUntil);

  _metricsRecorder = [[LensOverlayMetricsRecorder alloc]
      initWithEntrypoint:entrypoint
      associatedWebState:_associatedTabHelper->GetWebState()];

  // The instance that creates the Lens UI designates itself as the command
  // handler for the associated tab.
  _associatedTabHelper->SetLensOverlayCommandsHandler(self);
  _associatedTabHelper->SetLensOverlayUIAttachedAndAlive(true);

  __weak __typeof(self) weakSelf = self;
  [self captureSnapshotWithCompletion:^(UIImage* snapshot) {
    [weakSelf onSnapshotCaptured:snapshot
                      entrypoint:entrypoint
                        animated:animated
                      completion:completion];
  }];
}

- (void)onSnapshotCaptured:(UIImage*)snapshot
                entrypoint:(LensOverlayEntrypoint)entrypoint
                  animated:(BOOL)animated
                completion:(void (^)(BOOL))completion {
  if (!snapshot) {
    if (completion) {
      completion(NO);
    }
    return;
  }

  BOOL success = [self createUIWithSnapshot:snapshot entrypoint:entrypoint];
  if (success) {
    [self showLensUI:animated];
  } else {
    [self destroyLensUI:NO
                 reason:lens::LensOverlayDismissalSource::
                            kErrorScreenshotCreationFailed];
  }

  if (completion) {
    completion(success);
  }
}

- (void)showLensUI:(BOOL)animated {
  if (!self.isUICreated || self.isLensOverlayVisible) {
    return;
  }

  [self lockOrientationInPortrait:YES];
  [_selectionViewController setTopIconsHidden:self.shouldShowConsentFlow];

  [_metricsRecorder setLensOverlayInForeground:YES];

  __weak __typeof(self) weakSelf = self;

  [self showRestorationWindowIfNeeded];
  [self.baseViewController
      presentViewController:_containerViewController
                   animated:animated
                 completion:^{
                   [weakSelf onContainerViewControllerPresented];
                 }];
}

- (void)lockOrientationInPortrait:(BOOL)portraitLock {
  AppState* appState = self.browser->GetSceneState().profileState.appState;
  if (portraitLock) {
    if (!appState) {
      return;
    }
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
  } else {
    _scopedForceOrientation.reset();
  }
}

- (void)onContainerViewControllerPresented {
  if (self.shouldShowConsentFlow) {
    if (self.isResultsBottomSheetOpen) {
      [self stopResultPage];
    }
    [self presentConsentFlow];
  } else if (self.isResultsBottomSheetOpen) {
    [self showResultsBottomSheet];
  }

  // The auxiliary window should be retained until the container is confirmed
  // presented to avoid visual flickering when swapping back the main window.
  if (_associatedTabHelper) {
    _associatedTabHelper->ReleaseSnapshotAuxiliaryWindows();
  }
}

- (void)presentConsentFlow {
  [self createConsentViewController];
  [self showConsentViewController];
  [_metricsRecorder recordPermissionRequestedToBeShown];
}

- (void)hideLensUI:(BOOL)animated {
  if (!self.isUICreated) {
    return;
  }

  _displayLink.paused = YES;
  [_metricsRecorder setLensOverlayInForeground:NO];
  _associatedTabHelper->UpdateSnapshotStorage();
  [self dismissRestorationWindow];
  [self lockOrientationInPortrait:NO];

  [_containerViewController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:nil];
}

- (void)destroyLensUI:(BOOL)animated
               reason:(lens::LensOverlayDismissalSource)dismissalSource {
  if (_isExiting) {
    return;
  }

  _isExiting = YES;

  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidReceiveMemoryWarningNotification
              object:nil];

  [_metricsRecorder
      recordDismissalMetricsWithSource:dismissalSource
                     generatedTabCount:_mediator.generatedTabCount];

  // The reason the UI is destroyed can be that Omnient gets associated to a
  // different tab. In this case mark the stale tab helper as not shown.
  if (_associatedTabHelper) {
    _associatedTabHelper->SetLensOverlayUIAttachedAndAlive(false);
    _associatedTabHelper->RecordSheetDimensionState(SheetDimensionStateHidden);
    _associatedTabHelper->ClearViewportSnapshot();
    _associatedTabHelper->UpdateSnapshot();
  }

  if (!animated) {
    [self exitWithoutAnimation];
    return;
  }

  // Taking the screenshot triggered fullscreen mode. Ensure it's reverted in
  // the cleanup process. Exiting fullscreen has to happen on destruction to
  // ensure a smooth transition back to the content.
  __weak __typeof(self) weakSelf = self;
  void (^onAnimationFinished)() = ^{
    [weakSelf dismissLensOverlayWithCompletion:^{
      [weakSelf destroyViewControllersAndMediators];
    }];
  };

  [self executeExitAnimationFlowWithCompletion:onAnimationFinished];
}

#pragma mark - Exit animations

- (void)exitWithoutAnimation {
  __weak __typeof(self) weakSelf = self;
  void (^completion)() = ^{
    [weakSelf exitFullscreenAnimated:NO];
    [weakSelf destroyViewControllersAndMediators];
  };

  UIViewController* presentingViewController =
      _containerViewController.presentingViewController;

  if (!presentingViewController) {
    completion();
    return;
  }

  [presentingViewController dismissViewControllerAnimated:NO
                                               completion:completion];
}

- (void)executeExitAnimationFlowWithCompletion:(void (^)())completion {
  __block int completionCount = 0;
  void (^onAnimationFinished)() = ^{
    completionCount++;
    if (completionCount == kExpectedExitAnimationCount) {
      if (completion) {
        completion();
      }
    }
  };

  [self animateBottomSheetExitWithCompletion:onAnimationFinished];
  [self animateSelectionUIExitWithCompletion:onAnimationFinished];
}

- (void)animateBottomSheetExitWithCompletion:(void (^)())completion {
  UIViewController* presentedViewController =
      _containerViewController.presentedViewController;
  if (!presentedViewController) {
    completion();
    return;
  }

  [presentedViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           base::SequencedTaskRunner::GetCurrentDefault()
                               ->PostTask(FROM_HERE,
                                          base::BindOnce(completion));
                         }];
}

- (void)animateSelectionUIExitWithCompletion:(void (^)())completion {
  __weak __typeof(self) weakSelf = self;
  [_selectionViewController resetSelectionAreaToInitialPosition:^{
    [weakSelf exitFullscreenAnimated:YES];
    [UIView animateWithDuration:kSelectionViewDismissAnimationDuration
        animations:^{
          __typeof(self) strongSelf = weakSelf;
          if (!strongSelf) {
            return;
          }
          strongSelf->_selectionViewController.view.alpha = 0;
        }
        completion:^(BOOL status) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(completion));
        }];
  }];
}

- (void)dismissLensOverlayWithCompletion:(void (^)())completion {
  UIViewController* presentingViewController =
      _containerViewController.presentingViewController;
  if (!presentingViewController) {
    completion();
    return;
  }

  [presentingViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           base::SequencedTaskRunner::GetCurrentDefault()
                               ->PostTask(FROM_HERE,
                                          base::BindOnce(completion));
                         }];
}

#pragma mark - LensOverlayPanTrackerDelegate

- (void)onPanGestureStarted:(LensOverlayPanTracker*)tracker {
  // NO-OP
}

- (void)onPanGestureEnded:(LensOverlayPanTracker*)tracker {
  if (tracker == _windowPanTracker) {
    // Keep peaking only for the duration of the gesture.
    if (_detentsManager.sheetDimension == SheetDimensionStatePeaking) {
      [_detentsManager
          adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
    }
  }
}

#pragma mark - LensOverlayNetworkIssueDelegate

- (void)onNetworkIssueAlertWillShow {
  // Only one view controller may be presented at a time, so dismiss the bottom
  // sheet.
  [self stopResultPage];
}

- (void)onNetworkIssueAlertAcknowledged {
  [self destroyLensUI:YES
               reason:lens::LensOverlayDismissalSource::kNetworkIssue];
}

#pragma mark - LensOverlayDetentsChangeObserver

- (void)onBottomSheetDimensionStateChanged:(SheetDimensionState)state {
  if (_associatedTabHelper) {
    _associatedTabHelper->RecordSheetDimensionState(state);
  }

  switch (state) {
    case SheetDimensionStateHidden:
      [self destroyLensUI:YES
                   reason:lens::LensOverlayDismissalSource::
                              kBottomSheetDismissed];
      break;
    case SheetDimensionStateLarge:
      [self disableSelectionInteraction:YES];
      break;
    case SheetDimensionStateConsent:
      break;
    default:
      [self disableSelectionInteraction:NO];
      [_mediator defocusOmnibox];
      break;
  }
}

- (BOOL)bottomSheetShouldDismissFromState:(SheetDimensionState)state {
  switch (state) {
    case SheetDimensionStateConsent:
    case SheetDimensionStateHidden:
      return YES;
    case SheetDimensionStatePeaking:
    case SheetDimensionStateLarge:
      return NO;
    case SheetDimensionStateMedium:
      // If the user is actively adjusting a selection (by moving the selection
      // frame), it means the sheet dismissal was incidental and shouldn't be
      // processed. Only when the sheet is directly dragged downwards should the
      // dismissal intent be considered.
      BOOL isSelecting = _selectionViewController.isPanningSelectionUI;
      if (isSelecting) {
        // Instead, when a touch collision is detected, go into the peak state.
        [_detentsManager adjustDetentsForState:SheetDetentStatePeakEnabled];
        return NO;
      }
      return YES;
  }
}

- (void)adjustSelectionOcclusionInsets {
  UIWindow* sceneWindow = self.browser->GetSceneState().window;
  if (!sceneWindow) {
    return;
  }

  // Pad the offset by a small ammount to avoid having the bottom edge of the
  // selection overlapped over the sheet.
  CGFloat estimatedMediumDetentHeight = sceneWindow.frame.size.height / 2;
  CGFloat offsetNeeded = estimatedMediumDetentHeight + kSelectionOffsetPadding;

  [_selectionViewController
      setOcclusionInsets:UIEdgeInsetsMake(0, 0, offsetNeeded, 0)
              reposition:YES
                animated:YES];
}

#pragma mark - LensOverlayMediatorDelegate

- (void)lensOverlayMediatorDidOpenOverlayMenu:(LensOverlayMediator*)mediator {
  [_metricsRecorder recordOverflowMenuOpened];
}

- (void)lensOverlayMediatorOpenURLInNewTabRequsted:(GURL)URL {
  // Take a snapshot of the current tab before opening the URL in a new tab.
  // A side effect of opening a new tab is that the snapshot storage associated
  // to the current web state is updated. This snapshot would not include the
  // bottom sheet in the view hierarchy. Refrain from commiting it to
  // the storage until the web state is marked hidden, as by that point all
  // other updates should be issued.
  _associatedTabHelper->RecordViewportSnaphot();
  _associatedTabHelper->RecordSheetDimensionState(
      _detentsManager.sheetDimension);
  if (IsLensOverlaySameTabNavigationEnabled()) {
    [_loadQueryHandler loadQuery:base::SysUTF8ToNSString(URL.spec())
                     immediately:YES];
  } else {
    [self openURLInNewTab:URL];
    [self showRestorationWindowIfNeeded];
  }
}

#pragma mark - LensOverlayResultConsumer

// This coordinator acts as a proxy consumer to the result consumer to implement
// lazy initialization of the result UI.
- (void)loadResultsURL:(GURL)url {
  [_metricsRecorder
      recordResultLoadedWithTextSelection:_mediator.currentLensResult
                                              .isTextSelection];

  if (!_resultMediator) {
    [self startResultPage];
  }

  [_resultMediator loadResultsURL:url];
}

- (void)handleSearchRequestStarted {
  [_resultMediator handleSearchRequestStarted];
}

- (void)handleSearchRequestErrored {
  if (_resultMediator) {
    [_resultMediator handleSearchRequestErrored];
  } else {
    [_networkIssueAlertPresenter showNoInternetAlert];
  }
}

- (void)handleSlowRequestHasStarted {
  if (!_resultMediator) {
    [self startResultPage];
  }
  [_resultMediator handleSlowRequestHasStarted];
}

#pragma mark - LensOverlayConsentViewControllerDelegate

- (void)didTapPrimaryActionButton {
  self.browser->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kLensOverlayConditionsAccepted, true);
  _consentViewController = nil;
  [_metricsRecorder recordPermissionsAccepted];

  __weak __typeof(self) weakSelf = self;
  [_containerViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf handleConsentViewControllerDismissed];
                         }];
}

- (void)didTapSecondaryActionButton {
  [_metricsRecorder recordPermissionsDenied];
  [self destroyLensUI:YES
               reason:lens::LensOverlayDismissalSource::kLensPermissionsDenied];
}

- (void)didPressLearnMore {
  [_metricsRecorder recordPermissionsLinkOpen];
  [self openURLInNewTab:GURL(kLearnMoreLensURL)];
}

#pragma mark - private

- (void)openURLInNewTab:(GURL)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:URL
                   inIncognito:self.browser->GetProfile()->IsOffTheRecord()];

  [HandlerForProtocol(self.browser->GetCommandDispatcher(), ApplicationCommands)
      openURLInNewTab:command];
}

// Lens needs to have visibility into the user's identity and whether the search
// should be incognito or not.
- (LensConfiguration*)createLensConfigurationForEntrypoint:
    (LensOverlayEntrypoint)entrypoint {
  Browser* browser = self.browser;
  LensConfiguration* configuration = [[LensConfiguration alloc] init];
  BOOL isIncognito = browser->GetProfile()->IsOffTheRecord();
  configuration.isIncognito = isIncognito;
  configuration.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();
  configuration.entrypoint = LensEntrypointFromOverlayEntrypoint(entrypoint);

  if (!isIncognito) {
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForProfile(browser->GetProfile());
    id<SystemIdentity> identity = authenticationService->GetPrimaryIdentity(
        ::signin::ConsentLevel::kSignin);
    configuration.identity = identity;
  }
  configuration.localState = GetApplicationContext()->GetLocalState();
  return configuration;
}

- (BOOL)shouldShowConsentFlow {
  return !self.termsOfServiceAccepted ||
         base::FeatureList::IsEnabled(kLensOverlayForceShowOnboardingScreen);
}

- (BOOL)termsOfServiceAccepted {
  return self.browser->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kLensOverlayConditionsAccepted);
}

- (void)startResultPage {
  Browser* browser = self.browser;
  ProfileIOS* profile = browser->GetProfile();

  web::WebState::CreateParams params = web::WebState::CreateParams(profile);
  web::WebStateDelegate* browserWebStateDelegate =
      WebStateDelegateBrowserAgent::FromBrowser(browser);
  _resultMediator = [[LensResultPageMediator alloc]
       initWithWebStateParams:params
      browserWebStateDelegate:browserWebStateDelegate
                 webStateList:browser->GetWebStateList()
                  isIncognito:profile->IsOffTheRecord()];
  _resultMediator.applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  _resultMediator.snackbarHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);
  _resultMediator.errorHandler = _networkIssueAlertPresenter;
  _resultMediator.delegate = _mediator;
  _mediator.resultConsumer = _resultMediator;

  _resultViewController = [[LensResultPageViewController alloc] init];
  _resultViewController.mutator = _resultMediator;
  _resultViewController.toolbarMutator = _mediator;

  _resultContextMenuProvider = [[ContextMenuConfigurationProvider alloc]
         initWithBrowser:browser
      baseViewController:_resultViewController
            baseWebState:_resultMediator.webState
           isLensOverlay:YES];
  _resultContextMenuProvider.delegate = _resultMediator;

  _resultMediator.consumer = _resultViewController;
  _resultMediator.webViewContainer = _resultViewController.webViewContainer;
  _resultMediator.contextMenuProvider = _resultContextMenuProvider;

  [self showResultsBottomSheet];

  // TODO(crbug.com/355179986): Implement omnibox navigation with
  // omnibox_delegate.
  auto omniboxClient = std::make_unique<LensOmniboxClient>(
      profile, feature_engagement::TrackerFactory::GetForProfile(profile),
      /*web_provider=*/_resultMediator,
      /*omnibox_delegate=*/_mediator);
  _mediator.omniboxClient = omniboxClient.get();

  _omniboxCoordinator = [[OmniboxCoordinator alloc]
      initWithBaseViewController:nil
                         browser:browser
                   omniboxClient:std::move(omniboxClient)
                   isLensOverlay:YES];

  // TODO(crbug.com/355179721): Add omnibox focus delegate.
  _omniboxCoordinator.presenterDelegate = _resultViewController;
  _omniboxCoordinator.isSearchOnlyUI = YES;
  [_omniboxCoordinator start];

  [_omniboxCoordinator.managedViewController
      willMoveToParentViewController:_resultViewController];
  [_resultViewController
      addChildViewController:_omniboxCoordinator.managedViewController];
  [_resultViewController setEditView:_omniboxCoordinator.editView];
  [_omniboxCoordinator.managedViewController
      didMoveToParentViewController:_resultViewController];

  [_omniboxCoordinator updateOmniboxState];

  _mediator.omniboxCoordinator = _omniboxCoordinator;
  _mediator.toolbarConsumer = _resultViewController;
  _omniboxCoordinator.focusDelegate = _mediator;
}

- (void)exitFullscreenAnimated:(BOOL)animated {
  Browser* browser = self.browser;
  if (!browser) {
    return;
  }

  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(browser);

  if (animated) {
    fullscreenController->ExitFullscreen();
  } else {
    fullscreenController->ExitFullscreenWithoutAnimation();
  }
}

- (void)stopResultPage {
  [_resultContextMenuProvider stop];
  _resultContextMenuProvider = nil;
  [_resultViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _resultViewController = nil;
  [_resultMediator disconnect];
  _resultMediator = nil;
  _mediator.resultConsumer = self;
  _mediator.omniboxClient = nil;
  [_omniboxCoordinator stop];
  _omniboxCoordinator = nil;
}

- (BOOL)isUICreated {
  return _containerViewController != nil;
}

- (BOOL)isResultsBottomSheetOpen {
  return _resultViewController != nil;
}

// Disconnect and destroy all of the owned view controllers.
- (void)destroyViewControllersAndMediators {
  [self stopResultPage];
  _containerViewController = nil;
  [_mediator disconnect];
  _selectionViewController = nil;
  _mediator = nil;
  _consentViewController = nil;
  _isExiting = NO;
  _associatedTabHelper = nil;
  _metricsRecorder = nil;
  [_displayLink invalidate];
  _displayLink = nil;
  _scopedForceOrientation.reset();
  _networkIssueAlertPresenter = nil;
}

// The tab helper for the active web state.
- (LensOverlayTabHelper*)activeTabHelper {
  if (!self.browser || !self.browser->GetWebStateList() ||
      !self.browser->GetWebStateList()->GetActiveWebState()) {
    return nullptr;
  }

  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  LensOverlayTabHelper* tabHelper =
      LensOverlayTabHelper::FromWebState(activeWebState);

  CHECK(tabHelper, kLensOverlayNotFatalUntil);

  return tabHelper;
}

- (UIAction*)openURLAction:(GURL)URL {
  BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:self.browser
             scenario:kMenuScenarioHistogramHistoryEntry];
  UIAction* action = [actionFactory actionToOpenInNewTabWithURL:URL
                                                     completion:nil];
  return action;
}

- (UIAction*)openUserActivityAction {
  UIAction* action = [self openURLAction:GURL(kMyActivityURL)];
  action.title = l10n_util::GetNSString(IDS_IOS_MY_ACTIVITY_TITLE);
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  action.image = MakeSymbolMonochrome(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kMenuSymbolSize));
#else
  action.image = nil;
#endif
  return action;
}

- (UIAction*)learnMoreAction {
  UIAction* action = [self openURLAction:GURL(kLearnMoreLensURL)];
  action.title = l10n_util::GetNSString(IDS_IOS_LENS_LEARN_MORE);
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  action.image = MakeSymbolMonochrome(
      DefaultSymbolWithPointSize(kInfoCircleSymbol, kMenuSymbolSize));
#else
  action.image = nil;
#endif
  return action;
}

// Captures a screenshot of the active web state.
- (void)captureSnapshotWithCompletion:(void (^)(UIImage*))completion {
  Browser* browser = self.browser;
  if (!browser) {
    completion(nil);
    return;
  }

  web::WebState* activeWebState =
      browser->GetWebStateList()->GetActiveWebState();

  if (!activeWebState) {
    completion(nil);
    return;
  }

  CHECK(_associatedTabHelper, kLensOverlayNotFatalUntil);

  UIWindow* sceneWindow = self.browser->GetSceneState().window;
  if (!sceneWindow) {
    completion(nil);
    return;
  }

  _associatedTabHelper->SetSnapshotController(
      std::make_unique<LensOverlaySnapshotController>(
          SnapshotTabHelper::FromWebState(activeWebState),
          FullscreenController::FromBrowser(browser), sceneWindow,
          IsCurrentLayoutBottomOmnibox(browser)));

  _associatedTabHelper->CaptureFullscreenSnapshot(base::BindOnce(completion));
}

- (void)lowMemoryWarningReceived {
  // Preserve the UI if it's currently visible to the user.
  if ([self isLensOverlayVisible]) {
    return;
  }

  [self destroyLensUI:NO reason:lens::LensOverlayDismissalSource::kLowMemory];
}

- (BOOL)isLensOverlayVisible {
  return _containerViewController.presentingViewController != nil;
}

- (void)showConsentViewController {
  [_metricsRecorder recordLensOverlayConsentShown];
  [self disableSelectionInteraction:YES];
  // Configure sheet presentation
  UISheetPresentationController* sheet =
      _consentViewController.sheetPresentationController;
  sheet.prefersEdgeAttachedInCompactHeight = YES;
  _detentsManager =
      [[LensOverlayDetentsManager alloc] initWithBottomSheet:sheet];
  _detentsManager.observer = self;
  [_detentsManager adjustDetentsForState:SheetDetentStateConsentDialog];

  [_containerViewController presentViewController:_consentViewController
                                         animated:YES
                                       completion:nil];
}

// Blocks user interaction with the Lens UI.
- (void)disableSelectionInteraction:(BOOL)disabled {
  _containerViewController.selectionInteractionDisabled = disabled;
  [_selectionViewController disableFlyoutMenu:disabled];
}

// Called after consent dialog was dismissed and TOS accepted.
- (void)handleConsentViewControllerDismissed {
  CHECK([self termsOfServiceAccepted]);
  [self disableSelectionInteraction:NO];
  [_selectionViewController setTopIconsHidden:NO];
  [_selectionViewController start];
}

- (void)showResultsBottomSheet {
  if (!_associatedTabHelper) {
    return;
  }

  UISheetPresentationController* sheet =
      _resultViewController.sheetPresentationController;
  sheet.prefersEdgeAttachedInCompactHeight = YES;
  sheet.prefersGrabberVisible = YES;
  sheet.preferredCornerRadius = 14;

  // Extract the restored state before showing the sheet to avoid having it
  // overwritten.
  SheetDimensionState restoredState =
      _associatedTabHelper->GetRecordedSheetDimensionState();

  _detentsManager =
      [[LensOverlayDetentsManager alloc] initWithBottomSheet:sheet];
  _detentsManager.observer = self;
  [_detentsManager adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
  _resultMediator.presentationDelegate = _detentsManager;
  _mediator.presentationDelegate = _detentsManager;

  BOOL isStateRestoration = restoredState != SheetDimensionStateHidden;
  if (restoredState == SheetDimensionStateLarge) {
    [self->_detentsManager requestMaximizeBottomSheet];
  }

  // Adjust the occlusion insets so that selections in the bottom half of the
  // screen are repositioned, to avoid being hidden by the bottom sheet.
  //
  // Note(crbug.com/370930119): The adjustment has to be done right before the
  // bottom sheet is presented. Otherwise the coachmark will appear displaced.
  // This is a known limitation on the Lens side, as there is currently no
  // independent way of adjusting the insets for the coachmark alone.
  [self adjustSelectionOcclusionInsets];

  // Presenting the bottom sheet adds a gesture recognizer on the main window
  // which in turn causes the touches on Lens Overlay to get canceled.
  // To prevent such a behavior, extract the recognizers added as a consequence
  // of presenting and allow touches to be delivered to views.
  __block NSSet<UIGestureRecognizer*>* panRecognizersBeforePresenting =
      [self panGestureRecognizersOnWindow];
  __weak __typeof(self) weakSelf = self;
  [_containerViewController
      presentViewController:_resultViewController
                   animated:!isStateRestoration
                 completion:^{
                   [weakSelf resultsBottomSheetPresented];
                   [weakSelf handlePanRecognizersAddedAfter:
                                 panRecognizersBeforePresenting];
                 }];
}

- (void)showRestorationWindowIfNeeded {
  // If there is a pending snapshot, show it in a separate fullscreen window to
  // ease the transition.
  UIWindow* sceneWindow = self.browser->GetSceneState().window;
  if (!_associatedTabHelper || !sceneWindow) {
    return;
  }
  UIImage* viewportSnapshot = _associatedTabHelper->GetViewportSnapshot();
  // If no snapshot was stored, it means that a restoration of state is not
  // needed.
  if (!viewportSnapshot) {
    return;
  }
  _restorationWindow =
      [[UIWindow alloc] initWithWindowScene:sceneWindow.windowScene];
  _restorationWindow.rootViewController =
      [[SnapshotCoverViewController alloc] initWithImage:viewportSnapshot];
  _restorationWindow.windowLevel = sceneWindow.windowLevel + 1;
  _restorationWindow.hidden = NO;
}

- (void)dismissRestorationWindow {
  _restorationWindow.hidden = YES;
  _restorationWindow = nil;
}

- (void)resultsBottomSheetPresented {
  [self dismissRestorationWindow];
  if (_associatedTabHelper) {
    _associatedTabHelper->ClearViewportSnapshot();
  }

  [self monitorResultsBottomSheetPosition];
}

- (void)monitorResultsBottomSheetPosition {
  UIWindow* sceneWindow = self.browser->GetSceneState().window;
  if (!sceneWindow) {
    return;
  }

  // Currently there is no system API for reactively obtaining the position of a
  // bottom sheet. For the lifetime of the LRP, use the display link to monitor
  // the position of it's frame relative to the container.

  // Invalidate any pre-existing display link before creating a new one.
  [_displayLink invalidate];
  _displayLink =
      [CADisplayLink displayLinkWithTarget:self
                                  selector:@selector(onDisplayLinkUpdate:)];
  [_displayLink addToRunLoop:[NSRunLoop currentRunLoop]
                     forMode:NSRunLoopCommonModes];

  _windowPanTracker = [[LensOverlayPanTracker alloc] initWithView:sceneWindow];
  _windowPanTracker.delegate = self;
  [_windowPanTracker startTracking];
}

- (void)onDisplayLinkUpdate:(CADisplayLink*)sender {
  if (!_resultViewController) {
    return;
  }

  CGRect presentedFrame = _resultViewController.view.frame;
  CGRect newFrame =
      [_resultViewController.view convertRect:presentedFrame
                                       toView:_containerViewController.view];
  CGFloat containerHeight = _containerViewController.view.frame.size.height;
  CGFloat currentSheetHeight = containerHeight - newFrame.origin.y;

  // Trigger the Lens UI exit flow when the release occurs below the threshold,
  // allowing the overlay animation to run concurrently with the sheet dismissal
  // one.
  BOOL sheetClosedThresholdReached =
      currentSheetHeight <= kThresholdHeightForClosingSheet;
  BOOL userTouchesTheScreen = _windowPanTracker.isPanning;
  BOOL shouldDestroyLensUI =
      sheetClosedThresholdReached && !userTouchesTheScreen;
  if (shouldDestroyLensUI) {
    [_displayLink invalidate];
    [_windowPanTracker stopTracking];
    [self
        destroyLensUI:YES
               reason:lens::LensOverlayDismissalSource::kBottomSheetDismissed];
  }
}

- (NSSet<UIPanGestureRecognizer*>*)panGestureRecognizersOnWindow {
  NSMutableSet<UIPanGestureRecognizer*>* panRecognizersOnWindow =
      [[NSMutableSet alloc] init];
  UIWindow* sceneWindow = self.browser->GetSceneState().window;
  if (!sceneWindow) {
    return panRecognizersOnWindow;
  }

  for (UIGestureRecognizer* recognizer in sceneWindow.gestureRecognizers) {
    if (recognizer &&
        [recognizer isKindOfClass:[UIPanGestureRecognizer class]]) {
      [panRecognizersOnWindow addObject:(UIPanGestureRecognizer*)recognizer];
    }
  }

  return panRecognizersOnWindow;
}

// Allow touches from gesture recognizers added by UIKit as a consequence of
// presenting a view controller.
- (void)handlePanRecognizersAddedAfter:
    (NSSet<UIGestureRecognizer*>*)panRecognizersBeforePresenting {
  NSMutableSet<UIGestureRecognizer*>* panRecognizersAfterPresenting =
      [[self panGestureRecognizersOnWindow] mutableCopy];
  [panRecognizersAfterPresenting minusSet:panRecognizersBeforePresenting];
  for (UIGestureRecognizer* recognizer in panRecognizersAfterPresenting) {
    recognizer.cancelsTouchesInView = NO;
  }
}

@end
