// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/timer/elapsed_timer.h"
#import "components/lens/lens_overlay_first_interaction_type.h"
#import "components/lens/lens_overlay_metrics.h"
#import "components/prefs/pref_service.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client_delegate.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator_delegate.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
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
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/omnibox/chrome_omnibox_client_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "ios/chrome/browser/web/model/web_state_delegate_browser_agent.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
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

// The expected number of animations happening at the same time when exiting.
const int kExpectedExitAnimationCount = 2;

// The duration of the dismiss animation when exiting the selection UI.
const CGFloat kSelectionViewDismissAnimationDuration = 0.2f;

NSString* const kCustomConsentSheetDetentIdentifier =
    @"kCustomConsentSheetDetentIdentifier";

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
const CGFloat kMenuSymbolSize = 18;
#endif

}  // namespace

// Indicates the state of the bottom sheet
typedef NS_ENUM(NSUInteger, SheetDetentState) {
  // The bottom sheet is locked in large detent.
  SheetStateLockedInLargeDetent,
  // The bottom sheet is free to oscilate between medium and large.
  SheetStateUnrestrictedMovement,
  // The bottom sheet is presenting the consent dialog sheet.
  SheetStateConsentDialog,
};

@interface LensOverlayCoordinator () <
    LensOverlayCommands,
    UISheetPresentationControllerDelegate,
    LensOverlayMediatorDelegate,
    LensOverlayResultConsumer,
    LensOverlayBottomSheetPresentationDelegate,
    LensOverlayConsentViewControllerDelegate>

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

  // View that blocks user interaction with selection UI when the consent view
  // controller is displayed. Note that selection UI isn't started, so it won't
  // accept many interactions, but we do this to be extra safe.
  UIView* _selectionInteractionBlockingView;

  /// Entrypoint used for the current lens overlay invocation.
  LensOverlayEntrypoint _currentEntrypoint;
  /// The time at which the overlay was invoked.
  base::ElapsedTimer _invocationTime;
  /// The time at which the overlay UI was `shown`. null when hidden.
  base::TimeTicks _foregroundTime;
  /// The total foregroud duration since invoked.
  base::TimeDelta _foregroundDuration;
  /// Whether a lens request has been performed during this session.
  BOOL _searchPerformedInSession;
  /// Whether the first interaction has been recorded.
  BOOL _firstInteractionRecorded;
  /// Indicates the Lens Overlay is in the exit flow.
  BOOL _isExiting;
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
  // The mediator might destroy lens UI if the search engine doesn't support
  // lens.
  _mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.browser->GetProfile());

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

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(lowMemoryWarningReceived)
             name:UIApplicationDidReceiveMemoryWarningNotification
           object:nil];

  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensOverlayCommands)];
}

- (void)stop {
  if (Browser* browser = self.browser) {
    [browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  }

  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidReceiveMemoryWarningNotification
              object:nil];

  [super stop];
}

#pragma mark - LensOverlayCommands

- (void)createAndShowLensUI:(BOOL)animated
                 entrypoint:(LensOverlayEntrypoint)entrypoint
                 completion:(void (^)(BOOL))completion {
  if ([self isUICreated]) {
    // The UI is probably associated with the non-active tab. Destroy it with no
    // animation.
    [self destroyLensUI:NO
                 reason:lens::LensOverlayDismissalSource::kNewLensInvocation];
  }
  _currentEntrypoint = entrypoint;
  _invocationTime = base::ElapsedTimer();
  _foregroundTime = base::TimeTicks();
  _foregroundDuration = base::TimeDelta();
  _searchPerformedInSession = NO;
  _firstInteractionRecorded = NO;

  _associatedTabHelper = [self activeTabHelper];
  CHECK(_associatedTabHelper, kLensOverlayNotFatalUntil);

  // The instance that creates the Lens UI designates itself as the command
  // handler for the associated tab.
  _associatedTabHelper->SetLensOverlayCommandsHandler(self);
  _associatedTabHelper->SetLensOverlayShown(true);

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
  if (![self isUICreated]) {
    return;
  }
  _foregroundTime = base::TimeTicks::Now();

  __weak __typeof(self) weakSelf = self;
  [self.baseViewController
      presentViewController:_containerViewController
                   animated:animated
                 completion:^{
                   [weakSelf onContainerViewControllerPresented];
                 }];
}

- (void)onContainerViewControllerPresented {
  BOOL shouldShowConsentFlow =
      !self.termsOfServiceAccepted ||
      base::FeatureList::IsEnabled(kLensOverlayForceShowOnboardingScreen);
  if (shouldShowConsentFlow) {
    if (self.isResultsBottomSheetOpen) {
      [self stopResultPage];
    }
    [self presentConsentFlow];
  } else if (self.isResultsBottomSheetOpen) {
    [self showResultsBottomSheet];
  }
}

- (void)presentConsentFlow {
  [self createConsentViewController];
  [self showConsentViewController];
  lens::RecordPermissionRequestedToBeShown(true, self.currentInvocationSource);
}

- (void)hideLensUI:(BOOL)animated {
  if (![self isUICreated]) {
    return;
  }
  // Add the foreground duration and reset the timer.
  _foregroundDuration =
      _foregroundDuration + (base::TimeTicks::Now() - _foregroundTime);
  _foregroundTime = base::TimeTicks();

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

  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.Closed"));
  [self recordDismissalMetrics:dismissalSource];

  // The reason the UI is destroyed can be that Omnient gets associated to a
  // different tab. In this case mark the stale tab helper as not shown.
  if (_associatedTabHelper) {
    _associatedTabHelper->SetLensOverlayShown(false);
    _associatedTabHelper->UpdateSnapshot();
    _associatedTabHelper = nil;
  }

  if (!animated) {
    [self exitWithoutAnimation];
    return;
  }

  // Taking the screenshot triggered fullscreen mode. Ensure it's reverted in
  // the cleanup process. Exiting fullscreen has to happen on destruction to
  // ensure a smooth transition back to the content.
  __weak __typeof(self) weakSelf = self;
  __block int completionCount = 0;
  void (^onAnimationFinished)() = ^{
    completionCount++;
    if (completionCount == kExpectedExitAnimationCount) {
      [weakSelf dismissLensOverlayWithCompletion:^{
        [weakSelf destroyViewControllersAndMediators];
      }];
    }
  };

  [self animateBottomSheetExitWithCompletion:onAnimationFinished];
  [self animateSelectionUIExitWithCompletion:onAnimationFinished];
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

#pragma mark - UISheetPresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  UIViewController* presentedViewController =
      presentationController.presentedViewController;

  if (presentedViewController == _consentViewController) {
    return YES;
  }

  CHECK_EQ(presentedViewController, _resultViewController);
  // Only allow swiping down to dismiss when not at the largest detent.
  UISheetPresentationController* sheet =
      base::apple::ObjCCastStrict<UISheetPresentationController>(
          presentationController);
  BOOL isInLargestDetent = [sheet.selectedDetentIdentifier
      isEqualToString:UISheetPresentationControllerDetentIdentifierLarge];

  // If the user is actively adjusting a selection (by moving the selection
  // frame), it means the sheet dismissal was incidental and shouldn't be
  // processed. Only when the sheet is directly dragged downwards should the
  // dismissal intent be considered.
  BOOL isSelecting = _selectionViewController.isPanningSelectionUI;

  if (isSelecting || isInLargestDetent) {
    return NO;
  }

  return YES;
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  UIViewController* presentedViewController =
      presentationController.presentedViewController;

  CHECK(presentedViewController == _resultViewController ||
        presentedViewController == _consentViewController);
  [self destroyLensUI:YES
               reason:lens::LensOverlayDismissalSource::kBottomSheetDismissed];
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
  [self
      recordFirstInteraction:lens::LensOverlayFirstInteractionType::kLensMenu];
}

#pragma mark - LensOverlayResultConsumer

// This coordinator acts as a proxy consumer to the result consumer to implement
// lazy initialization of the result UI.
- (void)loadResultsURL:(GURL)url {
  DCHECK(!_resultMediator);

  // Time to first interaction metrics.
  if (!_searchPerformedInSession) {
    _searchPerformedInSession = YES;
    [self recordFirstInteraction:
              _mediator.currentLensResult.isTextSelection
                  ? lens::LensOverlayFirstInteractionType::kTextSelect
                  : lens::LensOverlayFirstInteractionType::kRegionSelect];
  }

  [self startResultPage];
  [_resultMediator loadResultsURL:url];
}

- (void)handleSearchRequestStarted {
  [_resultMediator handleSearchRequestStarted];
}

- (void)handleSearchRequestErrored {
  [_resultMediator handleSearchRequestErrored];
  [self showNoInternetAlert];
}

#pragma mark - LensOverlayConsentViewControllerDelegate

- (void)didTapPrimaryActionButton {
  self.browser->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kLensOverlayConditionsAccepted, true);
  _consentViewController = nil;
  lens::RecordPermissionUserAction(
      lens::LensPermissionUserAction::kAcceptButtonPressed,
      self.currentInvocationSource);
  [self recordFirstInteraction:lens::LensOverlayFirstInteractionType::
                                   kPermissionDialog];

  __weak __typeof(self) weakSelf = self;
  [_containerViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf handleConsentViewControllerDismissed];
                         }];
}

- (void)didTapSecondaryActionButton {
  lens::RecordPermissionUserAction(
      lens::LensPermissionUserAction::kCancelButtonPressed,
      self.currentInvocationSource);
  [self recordFirstInteraction:lens::LensOverlayFirstInteractionType::
                                   kPermissionDialog];
  [self destroyLensUI:YES
               reason:lens::LensOverlayDismissalSource::kLensPermissionsDenied];
}

- (void)didPressLearnMore {
  lens::RecordPermissionUserAction(lens::LensPermissionUserAction::kLinkOpened,
                                   self.currentInvocationSource);
  [self recordFirstInteraction:lens::LensOverlayFirstInteractionType::
                                   kPermissionDialog];
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:GURL(kLearnMoreLensURL)
                   inIncognito:self.browser->GetProfile()->IsOffTheRecord()];

  [HandlerForProtocol(self.browser->GetCommandDispatcher(), ApplicationCommands)
      openURLInNewTab:command];
}

#pragma mark - LensOverlayBottomSheetPresentationDelegate

- (void)requestMaximizeBottomSheet {
  CHECK(_containerViewController.presentedViewController ==
        _resultViewController);
  UISheetPresentationController* sheet =
      _resultViewController.sheetPresentationController;

  [sheet animateChanges:^{
    sheet.selectedDetentIdentifier =
        [UISheetPresentationControllerDetent largeDetent].identifier;
  }];
}

- (void)requestMinimizeBottomSheet {
  CHECK(_containerViewController.presentedViewController ==
        _resultViewController);
  UISheetPresentationController* sheet =
      _resultViewController.sheetPresentationController;

  [sheet animateChanges:^{
    sheet.selectedDetentIdentifier =
        [UISheetPresentationControllerDetent mediumDetent].identifier;
  }];
}

- (void)restrictSheetToLargeDetent:(BOOL)restrictToLargeDetent {
  UISheetPresentationController* sheet =
      _resultViewController.sheetPresentationController;

  if (restrictToLargeDetent) {
    [self requestMaximizeBottomSheet];
    [self adjustDetentsOfBottomSheet:sheet
                            forState:SheetStateLockedInLargeDetent];
  } else {
    [self adjustDetentsOfBottomSheet:sheet
                            forState:SheetStateUnrestrictedMovement];
  }
}

#pragma mark - private

- (void)showNoInternetAlert {
  if (!_containerViewController) {
    return;
  }

  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(IDS_IOS_LENS_ALERT_TITLE)
                       message:l10n_util::GetNSString(
                                   IDS_IOS_LENS_ALERT_SUBTITLE)
                preferredStyle:UIAlertControllerStyleAlert];

  __weak __typeof(self) weakSelf = self;
  UIAlertAction* defaultAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_LENS_ALERT_CLOSE_ACTION)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                [weakSelf destroyLensUI:YES
                                 reason:lens::LensOverlayDismissalSource::
                                            kLensPermissionsDenied];
              }];
  [alert addAction:defaultAction];

  [_containerViewController presentViewController:alert
                                         animated:YES
                                       completion:nil];
}

// Adjust the detents of the given sheet based on the sheet state.
- (void)adjustDetentsOfBottomSheet:(UISheetPresentationController*)sheet
                          forState:(SheetDetentState)state {
  if (!sheet) {
    return;
  }

  UISheetPresentationControllerDetent* largeDetent =
      [UISheetPresentationControllerDetent largeDetent];
  UISheetPresentationControllerDetent* mediumDetent =
      [UISheetPresentationControllerDetent mediumDetent];

  switch (state) {
    case SheetStateUnrestrictedMovement:
      sheet.detents = @[ mediumDetent, largeDetent ];
      break;
    case SheetStateLockedInLargeDetent:
      sheet.detents = @[ largeDetent ];
      break;
    case SheetStateConsentDialog:
      __weak UIViewController* weakConsentController = _consentViewController;
      auto preferredHeightForContent = ^CGFloat(
          id<UISheetPresentationControllerDetentResolutionContext> context) {
        return weakConsentController.preferredContentSize.height;
      };
      UISheetPresentationControllerDetent* consentDialogDetent =
          [UISheetPresentationControllerDetent
              customDetentWithIdentifier:kCustomConsentSheetDetentIdentifier
                                resolver:preferredHeightForContent];
      sheet.detents = @[ consentDialogDetent ];
      break;
  }

  sheet.largestUndimmedDetentIdentifier = largeDetent.identifier;
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
  _resultMediator.delegate = _mediator;
  _resultMediator.presentationDelegate = self;
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
  _mediator.presentationDelegate = self;
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

  _associatedTabHelper->SetSnapshotController(
      std::make_unique<LensOverlaySnapshotController>(
          SnapshotTabHelper::FromWebState(activeWebState),
          FullscreenController::FromBrowser(browser)));

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
  return self.baseViewController.presentedViewController != nil;
}

- (void)showConsentViewController {
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.Consent.Show"));
  // Block user interaction with the lens UI
  UIView* containerView = _containerViewController.view;
  UIView* blocker = [[UIView alloc] init];
  blocker.backgroundColor = UIColor.clearColor;
  blocker.userInteractionEnabled = YES;
  [containerView addSubview:blocker];

  blocker.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(containerView, blocker);
  _selectionInteractionBlockingView = blocker;

  // Configure sheet presentation
  UISheetPresentationController* sheet =
      _consentViewController.sheetPresentationController;
  sheet.delegate = self;
  sheet.prefersEdgeAttachedInCompactHeight = YES;
  [self adjustDetentsOfBottomSheet:sheet forState:SheetStateConsentDialog];

  [_containerViewController presentViewController:_consentViewController
                                         animated:YES
                                       completion:nil];
}

// Called after consent dialog was dismissed and TOS accepted.
- (void)handleConsentViewControllerDismissed {
  CHECK([self termsOfServiceAccepted]);
  [_selectionInteractionBlockingView removeFromSuperview];
  [_selectionViewController start];
}

- (void)showResultsBottomSheet {
  UISheetPresentationController* sheet =
      _resultViewController.sheetPresentationController;
  sheet.delegate = self;
  sheet.prefersEdgeAttachedInCompactHeight = YES;
  sheet.prefersGrabberVisible = YES;
  sheet.preferredCornerRadius = 14;
  [self adjustDetentsOfBottomSheet:sheet
                          forState:SheetStateUnrestrictedMovement];

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
                   animated:YES
                 completion:^{
                   [weakSelf handlePanRecognizersAddedAfter:
                                 panRecognizersBeforePresenting];
                 }];
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

/// Converts the current entrypoint to LensOverlayInvocationSource.
- (lens::LensOverlayInvocationSource)currentInvocationSource {
  return lens::InvocationSourceFromEntrypoint(_currentEntrypoint);
}

/// Returns the UKM source id from the associated tab.
- (ukm::SourceId)associatedTabSourceId {
  if (_associatedTabHelper) {
    if (web::WebState* webState = _associatedTabHelper->GetWebState()) {
      return ukm::GetSourceIdForWebStateDocument(webState);
    }
  }
  return ukm::kInvalidSourceId;
}

/// Records the first interaction time.
- (void)recordFirstInteraction:
    (lens::LensOverlayFirstInteractionType)firstInteractionType {
  if (_firstInteractionRecorded) {
    return;
  }
  _firstInteractionRecorded = YES;
  lens::RecordTimeToFirstInteraction(
      self.currentInvocationSource, _invocationTime.Elapsed(),
      firstInteractionType, self.associatedTabSourceId);
}

/// Metrics recorded on lens overlay dismissal.
- (void)recordDismissalMetrics:
    (lens::LensOverlayDismissalSource)dismissalSource {
  lens::LensOverlayInvocationSource invocationSource =
      self.currentInvocationSource;

  // First interaction metrics.
  [self recordFirstInteraction:lens::LensOverlayFirstInteractionType::kClose];

  // Invocation metrics.
  lens::RecordInvocation(invocationSource);
  lens::RecordInvocationResultedInSearch(invocationSource,
                                         _searchPerformedInSession);
  // Dismissal metric.
  lens::RecordDismissal(dismissalSource);

  // Session foreground duration metrics.
  if (_foregroundTime != base::TimeTicks()) {
    _foregroundDuration =
        _foregroundDuration + (base::TimeTicks::Now() - _foregroundTime);
  }
  lens::RecordSessionForegroundDuration(invocationSource, _foregroundDuration);

  // Session duration metrics.
  base::TimeDelta sessionDuration = _invocationTime.Elapsed();
  lens::RecordSessionDuration(invocationSource, sessionDuration);

  // Records number of tabs opened by the lens overlay during session.
  lens::RecordGeneratedTabCount(_mediator.generatedTabCount);

  // Session end UKM metrics.
  lens::RecordUKMSessionEndMetrics(
      self.associatedTabSourceId, self.currentInvocationSource,
      _searchPerformedInSession, sessionDuration, _foregroundDuration,
      _mediator.generatedTabCount);
}

@end
