// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"

#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_content_entry_point.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller_delegate.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_metrics.h"
#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_mediator.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_controls_view.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_view_controller.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

@interface PageActionMenuCoordinator () <
    AccountMenuCoordinatorDelegate,
    PageActionMenuViewControllerDelegate,
    UINavigationControllerDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

namespace {
// Interval for polling the workspace policy check status.
constexpr NSTimeInterval kEligibilityPollInterval = 0.3;
// Maximum time to wait for the workspace policy check before falling back
// to starting Gemini optimistically.
constexpr NSTimeInterval kEligibilityPollTimeout = 5.0;
}  // namespace

@implementation PageActionMenuCoordinator {
  UINavigationController* _navigationController;
  PageActionMenuViewController* _viewController;
  PageActionMenuMediator* _mediator;
  // Reader mode view controller and mediator.
  ReaderModeOptionsViewController* _readerModeOptionsViewController;
  ReaderModeOptionsMediator* _readerModeOptionsMediator;
  // The sign-in coordinator presented when a signed-out user taps Ask Gemini.
  SigninCoordinator* _signinCoordinator;
  // The account menu coordinator for switching accounts when the current
  // account is ineligible for Gemini (workspace restriction).
  AccountMenuCoordinator* _accountMenuCoordinator;
  // Timer that polls for workspace policy check completion after sign-in.
  NSTimer* _eligibilityPollTimer;
}

#pragma mark - ChromeCoordinator

- (void)start {
  raw_ptr<GeminiService> geminiService =
      GeminiServiceFactory::GetForProfile(self.profile);
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  _viewController = [[PageActionMenuViewController alloc] init];

  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(activeWebState);
  GeminiTabHelper* geminiTabHelper =
      GeminiTabHelper::FromWebState(activeWebState);

  HostContentSettingsMap* hostContentSettingsMap =
      ios::HostContentSettingsMapFactory::GetForProfile(self.profile);
  _mediator = [[PageActionMenuMediator alloc]
            initWithWebState:activeWebState
       authenticationService:AuthenticationServiceFactory::GetForProfile(
                                 self.profile)
          profilePrefService:self.profile->GetPrefs()
          templateURLService:ios::TemplateURLServiceFactory::GetForProfile(
                                 self.profile)
               geminiService:geminiService
             geminiTabHelper:geminiTabHelper
         readerModeTabHelper:readerModeTabHelper
      hostContentSettingsMap:hostContentSettingsMap];

  id<PageActionMenuCommands> pageActionMenuHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PageActionMenuCommands);
  _mediator.pageActionMenuHandler = pageActionMenuHandler;
  _mediator.consumer = _viewController;
  _mediator.contextualSheetHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ContextualSheetCommands);
  // Trigger the workspace policy check if it was skipped at init time.
  // This ensures eligibility data is available or loading by the time
  // the user interacts with Ask Gemini. The spinner handles the case
  // where the check is still in flight.
  geminiService->CheckGeminiEnterpriseEligibilityIfNeeded();

  if (readerModeTabHelper) {
    DistillerService* distillerService =
        DistillerServiceFactory::GetForProfile(self.profile);
    _readerModeOptionsMediator = [[ReaderModeOptionsMediator alloc]
        initWithDistilledPagePrefs:distillerService->GetDistilledPagePrefs()];
  }

  _viewController.delegate = self;
  _viewController.mutator = _mediator;

  _viewController.readerModeHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ReaderModeCommands);
  _viewController.pageActionMenuHandler = pageActionMenuHandler;
  if ([_mediator isUserSignedIn]) {
    _viewController.BWGHandler =
        HandlerForProtocol(self.browser->GetCommandDispatcher(), BWGCommands);
  }

  // If Lens is not available for the profile, then the handler has not been
  // configured.
  if ([_mediator isLensAvailableForProfile]) {
    _viewController.lensOverlayHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), LensOverlayCommands);
  }

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.view.accessibilityIdentifier =
      kAIHubBottomSheetAccessibilityIdentifier;
  _navigationController.delegate = self;
  _navigationController.presentationController.delegate = self;
  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;

  // Configure presentation sheet.
  __weak __typeof(self) weakSelf = self;
  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf resolveDetentValueForSheetPresentation:context];
  };
  UISheetPresentationControllerDetent* initialDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kAIHubDetentIdentifier
                            resolver:detentResolver];
  _navigationController.sheetPresentationController.detents = @[
    initialDetent,
  ];
  _navigationController.sheetPresentationController.selectedDetentIdentifier =
      kAIHubDetentIdentifier;
  _navigationController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;
  _navigationController.sheetPresentationController.prefersGrabberVisible = NO;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];

  // For managed accounts, if the workspace policy check is still pending,
  // show a spinner on the Ask Gemini button until the check completes.
  if (IsPageActionMenuAuthFlowEnabled() &&
      [_mediator isGeminiEligibilityLoading] && [_mediator isManagedAccount]) {
    [_viewController updateGeminiLoadingState:YES];
    [self startManagedAccountEligibilityPolling];
  }

  [super start];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

#pragma mark - Public

- (void)stopWithCompletion:(ProceduralBlock)completion {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES
                                                completion:completion];
  }
  _viewController = nil;
  [_mediator disconnect];
  _mediator = nil;
  _readerModeOptionsViewController = nil;
  [_readerModeOptionsMediator disconnect];
  _readerModeOptionsMediator = nil;
  [_signinCoordinator stop];
  _signinCoordinator = nil;
  [self stopEligibilityPolling];
  [self stopAccountMenu];
  [super stop];
}

#pragma mark - PageActionMenuViewControllerDelegate

- (void)viewControllerDidTapReaderModeOptionsButton:
    (PageActionMenuViewController*)viewController {
  _readerModeOptionsViewController =
      [[ReaderModeOptionsViewController alloc] init];
  [_readerModeOptionsViewController updateHideReaderModeButtonVisibility:NO];
  _readerModeOptionsViewController.readerModeOptionsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         ReaderModeOptionsCommands);
  _readerModeOptionsViewController.mutator = _readerModeOptionsMediator;
  _readerModeOptionsViewController.controlsView.mutator =
      _readerModeOptionsMediator;
  [_navigationController pushViewController:_readerModeOptionsViewController
                                   animated:YES];
}

- (void)viewControllerDidTapTranslateOptionsButton:
    (PageActionMenuViewController*)viewController {
  __weak __typeof(self) weakSelf = self;
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    [strongSelf->_mediator openTranslateOptions];
  }];
}

- (void)viewControllerDidTapSignedOutGemini:
    (PageActionMenuViewController*)viewController {
  signin_metrics::PromoAction promoAction =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  _signinCoordinator = [SigninCoordinator
      signinAndHistorySyncCoordinatorWithBaseViewController:
          _navigationController
                                                    browser:self.browser
                                               contextStyle:SigninContextStyle::
                                                                kDefault
                                                accessPoint:
                                                    signin_metrics::
                                                        AccessPoint::
                                                            kIosPageActionMenu
                                                promoAction:promoAction
                                        optionalHistorySync:YES
                                            fullscreenPromo:NO
                                       continuationProvider:
                                           DoNothingContinuationProvider()];
  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        [weakSelf signinDidFinishWithCoordinator:coordinator result:result];
      };
  [_signinCoordinator start];
}

- (void)viewController:(PageActionMenuViewController*)viewController
    didTapFooterItemLink:(NSString*)actionIdentifier {
  if ([actionIdentifier
          isEqualToString:kSearchEngineSettingsActionIdentifier]) {
    // Capture the handler before dismissal tears down the coordinator.
    id<SettingsCommands> settingsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), SettingsCommands);
    [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
      [settingsHandler showDefaultSearchEngineSettings];
    }];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordAIHubAction(IOSAIHubAction::kDismiss);
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  if (viewController == _viewController) {
    // If `_viewController` will be shown then it means that is was just pushed
    // as the root view controller, or `_readerModeOptionsViewController` was
    // just popped. In any case, `_readerModeOptionsViewController` can be set
    // to nil so it can be freed.
    _readerModeOptionsViewController = nil;
    _readerModeOptionsMediator.consumer = _viewController;
  } else {
    _readerModeOptionsMediator.consumer =
        _readerModeOptionsViewController.controlsView;
  }
  // Invalidate detents.
  [navigationController.sheetPresentationController animateChanges:^{
    [navigationController.sheetPresentationController invalidateDetents];
  }];
}

#pragma mark - AccountMenuCoordinatorDelegate

- (void)accountMenuCoordinatorWantsToBeStopped:
    (AccountMenuCoordinator*)coordinator {
  [self stopAccountMenu];

  raw_ptr<GeminiService> geminiService =
      GeminiServiceFactory::GetForProfile(self.profile);

  // Re-check eligibility after the account menu closes.
  if ([_mediator isUserSignedIn] &&
      geminiService->IsProfileEligibleForGemini()) {
    [self startGeminiSession];
    return;
  }

  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
}

#pragma mark - Private

// Returns the appropriate detent value for a sheet presentation in `context`.
- (CGFloat)resolveDetentValueForSheetPresentation:
    (id<UISheetPresentationControllerDetentResolutionContext>)context {
  // TODO(crbug.com/432213672): Add a protocol with
  // `resolveDetentValueForSheetPresentation:`.
  if (_readerModeOptionsViewController ==
      _navigationController.topViewController) {
    return [_readerModeOptionsViewController
        resolveDetentValueForSheetPresentation:context];
  }
  return [_viewController resolveDetentValueForSheetPresentation:context];
}

// Cleans up the sign-in coordinator after completion. On successful sign-in,
// dismisses the Page Action Menu and starts the Gemini flow.
- (void)signinDidFinishWithCoordinator:(SigninCoordinator*)coordinator
                                result:(SigninCoordinatorResult)result {
  CHECK_EQ(_signinCoordinator, coordinator);
  [_signinCoordinator stop];
  _signinCoordinator = nil;

  if (result != SigninCoordinatorResultSuccess) {
    [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
    return;
  }

  // If the workspace policy check is still in flight, show a spinner on
  // the Ask Gemini button and poll until the check completes.
  if ([_mediator isGeminiEligibilityLoading]) {
    [_viewController updateGeminiLoadingState:YES];
    [self startEligibilityPolling];
    return;
  }

  // Eligibility data is available: route immediately.
  [self routeAfterSignIn];
}

// Starts polling for workspace policy check completion.
- (void)startEligibilityPolling {
  __weak __typeof(self) weakSelf = self;
  NSDate* startTime = [NSDate date];
  _eligibilityPollTimer = [NSTimer
      scheduledTimerWithTimeInterval:kEligibilityPollInterval
                             repeats:YES
                               block:^(NSTimer* timer) {
                                 [weakSelf
                                     checkEligibilityWithStartTime:startTime];
                               }];
}

// Called by the poll timer. Checks if eligibility has resolved or timed out.
- (void)checkEligibilityWithStartTime:(NSDate*)startTime {
  NSTimeInterval elapsed = -[startTime timeIntervalSinceNow];

  // Timed out: fall back to starting Gemini optimistically.
  if (elapsed >= kEligibilityPollTimeout) {
    [self stopEligibilityPolling];
    [_viewController updateGeminiLoadingState:NO];
    [self startGeminiSession];
    return;
  }

  // Still loading: keep polling.
  if ([_mediator isGeminiEligibilityLoading]) {
    return;
  }

  // Resolved: route based on eligibility.
  [self stopEligibilityPolling];
  [_viewController updateGeminiLoadingState:NO];
  [self routeAfterSignIn];
}

// Stops the eligibility poll timer.
- (void)stopEligibilityPolling {
  [_eligibilityPollTimer invalidate];
  _eligibilityPollTimer = nil;
}

// Routes to the correct flow after eligibility data is available.
- (void)routeAfterSignIn {
  PageActionMenuContentEntryPoint* entryPoint = [_mediator geminiEntryPoint];

  if (entryPoint.enabled) {
    [self startGeminiSession];
    return;
  }

  // Workspace restriction on personal account: show snackbar and present
  // account menu for account switching.
  if ([_mediator isIneligibleGeminiAccountSwitchable]) {
    RecordAIHubAction(IOSAIHubAction::kGeminiIneligible);
    id<SnackbarCommands> snackbarHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), SnackbarCommands);
    SnackbarMessage* message = [[SnackbarMessage alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_AI_HUB_INELIGIBLE_ACCOUNT_SNACKBAR)];
    [snackbarHandler showSnackbarMessage:message];
    [self presentAccountMenu];
    return;
  }

  // If the only block is account_capability, it may be stale (capabilities
  // not loaded yet after sign-in). Start Gemini optimistically and the server
  // handles truly ineligible accounts. Enterprise managed accounts trigger
  // a profile switch before reaching here.
  [self startGeminiSession];
}

// Dismisses the PAM and starts the Gemini session.
- (void)startGeminiSession {
  id<BWGCommands> geminiHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), BWGCommands);
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:^{
    [geminiHandler
        startGeminiFlowWithStartupState:
            [[GeminiStartupState alloc]
                initWithEntryPoint:gemini::EntryPoint::AIHubSignInSheet]];
  }];
}

// Presents the account menu for switching to a different account.
- (void)presentAccountMenu {
  _accountMenuCoordinator = [[AccountMenuCoordinator alloc]
      initWithBaseViewController:_navigationController
                         browser:self.browser
                      anchorView:_navigationController.view
                     accessPoint:AccountMenuAccessPoint::kPageActionMenu
                             URL:GURL()];
  _accountMenuCoordinator.delegate = self;
  [_accountMenuCoordinator start];
}

// Stops and releases the account menu coordinator.
- (void)stopAccountMenu {
  [_accountMenuCoordinator stop];
  _accountMenuCoordinator = nil;
}

// Polls until the workspace policy check resolves, then updates the
// Ask Gemini button state. Used only for managed accounts when the PAM
// opens and the check is still in flight.
- (void)startManagedAccountEligibilityPolling {
  __weak __typeof(self) weakSelf = self;
  NSDate* startTime = [NSDate date];
  _eligibilityPollTimer = [NSTimer
      scheduledTimerWithTimeInterval:kEligibilityPollInterval
                             repeats:YES
                               block:^(NSTimer* timer) {
                                 [weakSelf
                                     pollManagedAccountEligibilityWithStartTime:
                                         startTime];
                               }];
}

// Called by the managed account poll timer. Hides the spinner when
// the eligibility check resolves or times out.
- (void)pollManagedAccountEligibilityWithStartTime:(NSDate*)startTime {
  NSTimeInterval elapsed = -[startTime timeIntervalSinceNow];
  if (elapsed < kEligibilityPollTimeout &&
      [_mediator isGeminiEligibilityLoading]) {
    return;
  }

  [self stopEligibilityPolling];
  [_viewController updateGeminiLoadingState:NO];
}

@end
