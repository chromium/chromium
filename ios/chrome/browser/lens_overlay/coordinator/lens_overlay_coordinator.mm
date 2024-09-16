// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client_delegate.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_web_state_delegate.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_consumer.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
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

const CGFloat kSelectionOffsetPadding = 50.0f;

NSString* const kCustomConsentSheetDetentIdentifier =
    @"kCustomConsentSheetDetentIdentifier";

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
const CGFloat kMenuSymbolSize = 18;
#endif

}  // namespace

@interface LensOverlayCoordinator () <
    LensOverlayCommands,
    UISheetPresentationControllerDelegate,
    LensOverlayResultConsumer,
    LensResultPageWebStateDelegate,
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
  LensOverlayTabHelper* _associatedTabHelper;

  /// Coordinator of the omnibox.
  OmniboxCoordinator* _omniboxCoordinator;

  LensOverlayConsentViewController* _consentViewController;

  UIViewController<ChromeLensOverlay>* _selectionViewController;

  // View that blocks user interaction with selection UI when the consent view
  // controller is displayed. Note that selection UI isn't started, so it won't
  // accept many interactions, but we do this to be extra safe.
  UIView* _selectionInteractionBlockingView;
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
  // The mediator might destory lens UI if the search engine doesn't support
  // lens.
  _mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

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
      UIModalPresentationOverFullScreen;
  _containerViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
}

- (void)createMediator {
  if (_mediator) {
    return;
  }
  _mediator = [[LensOverlayMediator alloc] init];

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
    [self destroyLensUI:NO];
  }

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
    completion(NO);
    return;
  }

  BOOL success = [self createUIWithSnapshot:snapshot entrypoint:entrypoint];
  if (success) {
    [self showLensUI:animated];
  } else {
    [self destroyLensUI:NO];
  }

  if (completion) {
    completion(success);
  }
}

- (void)showLensUI:(BOOL)animated {
  if (![self isUICreated]) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self.baseViewController
      presentViewController:_containerViewController
                   animated:animated
                 completion:^{
                   [weakSelf onContainerViewControllerPresented];
                 }];
}

- (void)onContainerViewControllerPresented {
  BOOL shouldShowConsentFlow = !self.termsOfServiceAccepted;
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
}

- (void)hideLensUI:(BOOL)animated {
  if (![self isUICreated]) {
    return;
  }

  [_containerViewController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:nil];
}

- (void)destroyLensUI:(BOOL)animated {
  // The reason the UI is destroyed can be that Omnient gets associated to a
  // different tab. In this case mark the stale tab helper as not shown.
  if (_associatedTabHelper) {
    _associatedTabHelper->SetLensOverlayShown(false);
    _associatedTabHelper->UpdateSnapshot();
    _associatedTabHelper = nil;
  }

  // Taking the screenshot triggered fullscreen mode. Ensure it's reverted in
  // the cleanup process. Exiting fullscreen has to happen on destruction to
  // ensure a smooth transition back to the content.
  __weak __typeof(self) weakSelf = self;
  if (_containerViewController.presentingViewController) {
    [self exitFullscreenAnimated:YES];
    [_containerViewController.presentingViewController
        dismissViewControllerAnimated:animated
                           completion:^{
                             [weakSelf destroyViewControllersAndMediators];
                           }];
  } else {
    [self exitFullscreenAnimated:NO];
    [self destroyViewControllersAndMediators];
  }
}

#pragma mark - UISheetPresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  UIViewController* presentedViewController =
      presentationController.presentedViewController;

  if (presentedViewController == _consentViewController) {
    // Don't let the consent view controller dismiss without user opting in or
    // out.
    return NO;
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

  if (presentedViewController == _resultViewController) {
    [self destroyLensUI:YES];
  }
}

- (void)adjustSelectionOcclusionInsets {
  if (!_resultViewController) {
    return;
  }

  UISheetPresentationController* sheet =
      _resultViewController.sheetPresentationController;
  UIViewController* presentedViewController = sheet.presentedViewController;
  // Pad the offset by a small ammount to avoid having the bottom edge of the
  // selection overlapped over the sheet.
  CGFloat sheetHeight = presentedViewController.view.frame.size.height;
  CGFloat offsetNeeded = sheetHeight + kSelectionOffsetPadding;

  [_selectionViewController
      setOcclusionInsets:UIEdgeInsetsMake(0, 0, offsetNeeded, 0)
              reposition:YES
                animated:YES];
}

#pragma mark - LensOverlayResultConsumer

// This coordinator acts as a proxy consumer to the result consumer to implement
// lazy initialization of the result UI.
- (void)loadResultsURL:(GURL)url {
  DCHECK(!_resultMediator);

  [self startResultPage];
  [_resultMediator loadResultsURL:url];
}

- (void)handleSearchRequestStarted {
  [_resultMediator handleSearchRequestStarted];
}

- (void)handleSearchRequestErrored {
  [_resultMediator handleSearchRequestErrored];
}

#pragma mark - LensResultPageWebStateDelegate

- (void)lensResultPageWebStateDestroyed {
  [self stopResultPage];
}

- (void)lensResultPageDidChangeActiveWebState:(web::WebState*)webState {
  _mediator.webState = webState;
}

#pragma mark - LensOverlayConsentViewControllerDelegate

- (void)consentViewController:(LensOverlayConsentViewController*)viewController
    didFinishWithTermsAccepted:(BOOL)accepted {
  self.browser->GetBrowserState()->GetPrefs()->SetBoolean(
      prefs::kLensOverlayConditionsAccepted, accepted);

  if (accepted) {
    // consentViewController is still presented, so the strong reference can be
    // removed here.
    _consentViewController = nil;

    __weak __typeof(self) weakSelf = self;
    [_containerViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf handleConsentViewControllerDismissed];
                           }];
  } else {
    [self destroyLensUI:YES];
  }
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

#pragma mark - private

// Lens needs to have visibility into the user's identity and whether the search
// should be incognito or not.
- (LensConfiguration*)createLensConfigurationForEntrypoint:
    (LensOverlayEntrypoint)entrypoint {
  Browser* browser = self.browser;
  LensConfiguration* configuration = [[LensConfiguration alloc] init];
  BOOL isIncognito = browser->GetBrowserState()->IsOffTheRecord();
  configuration.isIncognito = isIncognito;
  configuration.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();
  configuration.entrypoint = LensEntrypointFromOverlayEntrypoint(entrypoint);

  if (!isIncognito) {
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForBrowserState(
            browser->GetBrowserState());
    id<SystemIdentity> identity = authenticationService->GetPrimaryIdentity(
        ::signin::ConsentLevel::kSignin);
    configuration.identity = identity;
  }

  return configuration;
}

- (BOOL)termsOfServiceAccepted {
  return self.browser->GetBrowserState()->GetPrefs()->GetBoolean(
      prefs::kLensOverlayConditionsAccepted);
}

- (void)startResultPage {
  Browser* browser = self.browser;
  ChromeBrowserState* browserState = browser->GetBrowserState();

  web::WebState::CreateParams params =
      web::WebState::CreateParams(browserState);
  web::WebStateDelegate* browserWebStateDelegate =
      WebStateDelegateBrowserAgent::FromBrowser(browser);
  _resultMediator = [[LensResultPageMediator alloc]
       initWithWebStateParams:params
      browserWebStateDelegate:browserWebStateDelegate
                 webStateList:browser->GetWebStateList()
                  isIncognito:browserState->IsOffTheRecord()];
  _resultMediator.applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  _resultMediator.snackbarHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);
  _resultMediator.webStateDelegate = self;
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
      browserState,
      feature_engagement::TrackerFactory::GetForBrowserState(browserState),
      /*web_provider=*/_resultMediator,
      /*omnibox_delegate=*/_mediator);

  _omniboxCoordinator = [[OmniboxCoordinator alloc]
      initWithBaseViewController:nil
                         browser:browser
                   omniboxClient:std::move(omniboxClient)];

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
  [_selectionViewController removeSelectionWithClearText:YES];
  [_resultContextMenuProvider stop];
  _resultContextMenuProvider = nil;
  [_resultViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _resultViewController = nil;
  [_resultMediator disconnect];
  _resultMediator = nil;
  _mediator.resultConsumer = self;
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
          FullscreenController::FromBrowser(browser),
          base::SequencedTaskRunner::GetCurrentDefault()));

  _associatedTabHelper->CaptureFullscreenSnapshot(base::BindOnce(completion));
}

- (void)lowMemoryWarningReceived {
  // Preserve the UI if it's currently visible to the user.
  if ([self isLensOverlayVisible]) {
    return;
  }

  [self destroyLensUI:NO];
}

- (BOOL)isLensOverlayVisible {
  return self.baseViewController.presentedViewController != nil;
}

- (void)showConsentViewController {
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
  sheet.largestUndimmedDetentIdentifier =
      [UISheetPresentationControllerDetent largeDetent].identifier;
  sheet.prefersGrabberVisible = YES;

  __weak LensOverlayConsentViewController* weakConsentViewController =
      _consentViewController;

  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakConsentViewController preferredContentSize].height;
  };

  UISheetPresentationControllerDetent* customDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kCustomConsentSheetDetentIdentifier
                            resolver:resolver];

  sheet.detents = @[ customDetent ];

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
  sheet.largestUndimmedDetentIdentifier =
      [UISheetPresentationControllerDetent largeDetent].identifier;
  sheet.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  sheet.prefersGrabberVisible = YES;
  sheet.preferredCornerRadius = 20;

  __weak __typeof(self) weakSelf = self;
  [_containerViewController
      presentViewController:_resultViewController
                   animated:YES
                 completion:^{
                   [weakSelf adjustSelectionOcclusionInsets];
                 }];
}

@end
