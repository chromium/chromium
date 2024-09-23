// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"

#import "base/notreached.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_promo_non_modal_metrics_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer_bridge.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

namespace {

// Default time interval to wait to show the promo after loading a webpage.
// This should allow any initial overlays to be presented first.
constexpr base::TimeDelta kShowPromoWebpageLoadWaitTime = base::Seconds(3);

// Default time interval to wait to show the promo after the share action is
// completed.
constexpr base::TimeDelta kShowPromoPostShareWaitTime = base::Seconds(1);

// Timeout before the promo is dismissed.
constexpr base::TimeDelta kPromoTimeout = base::Seconds(45);

typedef NS_ENUM(NSUInteger, PromoReason) {
  PromoReasonNone,
  PromoReasonOmniboxPaste,
  PromoReasonExternalLink,
  PromoReasonShare
};

NonModalPromoTriggerType MetricTypeForPromoReason(PromoReason reason) {
  switch (reason) {
    case PromoReasonNone:
      return NonModalPromoTriggerType::kUnknown;
    case PromoReasonOmniboxPaste:
      return NonModalPromoTriggerType::kPastedLink;
    case PromoReasonExternalLink:
      return NonModalPromoTriggerType::kGrowthKitOpen;
    case PromoReasonShare:
      return NonModalPromoTriggerType::kShare;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

}  // namespace

@interface NonModalDefaultBrowserPromoSchedulerSceneAgent () <
    WebStateListObserving,
    CRWWebStateObserver,
    OverlayPresenterObserving,
    BrowserObserving> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;
  std::unique_ptr<OverlayPresenterObserverBridge> _overlayObserver;
  // Observe the browser the web state list is tied to to deregister any
  // observers before the browser is destroyed.
  std::unique_ptr<BrowserObserverBridge> _browserObserver;

  // Timer for showing the promo after page load.
  std::unique_ptr<base::OneShotTimer> _showPromoTimer;

  // Timer for dismissing the promo after it is shown.
  std::unique_ptr<base::OneShotTimer> _dismissPromoTimer;

  __weak id<DefaultBrowserPromoNonModalCommands> _handler;
  NSInteger _userInteractionWithNonModalPromoCount;
  NSInteger _displayedFullscreenPromoCount;
}

// Time when a promo was shown on screen, used for metrics only.
@property(nonatomic) base::TimeTicks promoShownTime;

// WebState that the triggering event occured in.
@property(nonatomic, assign) web::WebState* webStateToListenTo;

// Whether or not the promo is currently showing.
@property(nonatomic, assign) BOOL promoIsShowing;

// The web state list used to listen to page load and
// WebState change events.
@property(nonatomic, assign) WebStateList* webStateList;

// The overlay presenter used to prevent the
// promo from showing over an overlay.
@property(nonatomic, assign) OverlayPresenter* overlayPresenter;

// The trigger reason for the in-progress promo flow.
@property(nonatomic, assign) PromoReason currentPromoReason;

// The browser that this scheduler uses to listen to events, such as page loads
// and overlay events
@property(nonatomic, assign) Browser* browser;

@end

@implementation NonModalDefaultBrowserPromoSchedulerSceneAgent

- (instancetype)init {
  if ((self = [super init])) {
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _overlayObserver = std::make_unique<OverlayPresenterObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  self.browser = nullptr;
}

- (void)logUserPastedInOmnibox {
  if (self.currentPromoReason != PromoReasonNone) {
    return;
  }

  if (![self promoCanBeDisplayed]) {
    return;
  }

  // This assumes that the currently active webstate is the one that the paste
  // occured in.
  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  // There should always be an active web state when pasting in the omnibox.
  if (!activeWebState) {
    return;
  }

  self.currentPromoReason = PromoReasonOmniboxPaste;

  // Store the pasted web state, so when that web state's page load finishes,
  // the promo can be shown.
  self.webStateToListenTo = activeWebState;
}

- (void)logUserFinishedActivityFlow {
  if (self.currentPromoReason != PromoReasonNone) {
    return;
  }

  if (![self promoCanBeDisplayed]) {
    return;
  }

  self.currentPromoReason = PromoReasonShare;
  [self startShowPromoTimer];
}

- (void)logUserEnteredAppViaFirstPartyScheme {
  if (self.currentPromoReason != PromoReasonNone) {
    return;
  }

  if (![self promoCanBeDisplayed]) {
    return;
  }

  self.currentPromoReason = PromoReasonExternalLink;

  // Store the current web state, so when that web state's page load finishes,
  // the promo can be shown.
  self.webStateToListenTo = self.webStateList->GetActiveWebState();
}

- (void)logPromoWasDismissed {
  self.currentPromoReason = PromoReasonNone;
  self.webStateToListenTo = nullptr;
  self.promoIsShowing = NO;
}

- (void)logTabGridEntered {
  [self dismissPromoAnimated:YES];
}

- (void)logPopupMenuEntered {
  [self dismissPromoAnimated:YES];
}

- (void)logUserPerformedPromoAction {
  [self logPromoAction:self.currentPromoReason
        promoShownTime:self.promoShownTime];
  self.promoShownTime = base::TimeTicks();
}

- (void)logUserDismissedPromo {
  [self logPromoUserDismiss:self.currentPromoReason
             promoShownTime:self.promoShownTime];
  self.promoShownTime = base::TimeTicks();
}

- (void)dismissPromoAnimated:(BOOL)animated {
  _dismissPromoTimer = nullptr;
  [self notifyHandlerDismissPromo:animated];
}

- (bool)promoCanBeDisplayed {
  if (IsChromeLikelyDefaultBrowser()) {
    return false;
  }

  if (IsNonModalDefaultBrowserPromoCooldownRefactorEnabled() &&
      UserInNonModalPromoCooldown()) {
    return false;
  }

  if (!IsNonModalDefaultBrowserPromoCooldownRefactorEnabled() &&
      UserInFullscreenPromoCooldown()) {
    return false;
  }

  NSInteger count = UserInteractionWithNonModalPromoCount();
  return count < GetNonModalDefaultBrowserPromoImpressionLimit();
}

- (void)notifyHandlerShowPromo {
  // The count of past non-modal promo interactions and fullscreen promo
  // displays is cached because multiple interactions may be logged for the
  // current non-modal promo impression. This makes sure we don't over-increment
  // the interactions count value.
  _userInteractionWithNonModalPromoCount =
      UserInteractionWithNonModalPromoCount();
  _displayedFullscreenPromoCount = DisplayedFullscreenPromoCount();

  [_handler showDefaultBrowserNonModalPromo];
}

- (void)notifyHandlerDismissPromo:(BOOL)animated {
  [_handler dismissDefaultBrowserNonModalPromoAnimated:animated];
}

- (void)onEnteringBackground:(PromoReason)currentPromoReason
              promoIsShowing:(bool)promoIsShowing {
  if (currentPromoReason != PromoReasonNone && !promoIsShowing) {
    LogNonModalPromoAction(NonModalPromoAction::kBackgroundCancel,
                           MetricTypeForPromoReason(currentPromoReason),
                           _userInteractionWithNonModalPromoCount);
  }
  [self cancelShowPromoTimer];
  [self dismissPromoAnimated:NO];
}

- (void)logPromoAppear:(PromoReason)currentPromoReason {
  LogNonModalPromoAction(NonModalPromoAction::kAppear,
                         MetricTypeForPromoReason(currentPromoReason),
                         _userInteractionWithNonModalPromoCount);
}

- (void)logPromoAction:(PromoReason)currentPromoReason
        promoShownTime:(base::TimeTicks)promoShownTime {
  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kActionButton);
  LogNonModalPromoAction(NonModalPromoAction::kAccepted,
                         MetricTypeForPromoReason(currentPromoReason),
                         _userInteractionWithNonModalPromoCount);
  LogNonModalTimeOnScreen(promoShownTime);
  LogUserInteractionWithNonModalPromo(_userInteractionWithNonModalPromoCount,
                                      _displayedFullscreenPromoCount);

  NSURL* settingsURL = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
  [[UIApplication sharedApplication] openURL:settingsURL
                                     options:@{}
                           completionHandler:nil];
}

- (void)logPromoUserDismiss:(PromoReason)currentPromoReason
             promoShownTime:(base::TimeTicks)promoShownTime {
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kDismiss);
  LogNonModalPromoAction(NonModalPromoAction::kDismiss,
                         MetricTypeForPromoReason(currentPromoReason),
                         _userInteractionWithNonModalPromoCount);
  LogNonModalTimeOnScreen(promoShownTime);
  LogUserInteractionWithNonModalPromo(_userInteractionWithNonModalPromoCount,
                                      _displayedFullscreenPromoCount);
}

- (void)logPromoTimeout:(PromoReason)currentPromoReason
         promoShownTime:(base::TimeTicks)promoShownTime {
  LogNonModalPromoAction(NonModalPromoAction::kTimeout,
                         MetricTypeForPromoReason(currentPromoReason),
                         _userInteractionWithNonModalPromoCount);
  LogNonModalTimeOnScreen(promoShownTime);
  LogUserInteractionWithNonModalPromo(_userInteractionWithNonModalPromoCount,
                                      _displayedFullscreenPromoCount);
}

#pragma mark - Accessors

- (void)setBrowser:(Browser*)browser {
  if (_browser) {
    self.webStateList = nullptr;
    self.overlayPresenter = nullptr;
    _handler = nil;
  }

  _browser = browser;

  if (_browser) {
    _browserObserver = std::make_unique<BrowserObserverBridge>(_browser, self);
    self.webStateList = _browser->GetWebStateList();
    self.overlayPresenter = OverlayPresenter::FromBrowser(
        _browser, OverlayModality::kInfobarBanner);
    _handler = HandlerForProtocol(browser->GetCommandDispatcher(),
                                  DefaultBrowserPromoNonModalCommands);
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _forwarder = nullptr;
  }
  _webStateList = webStateList;
  if (_webStateList) {
    _webStateList->AddObserver(_webStateListObserver.get());
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        _webStateList, _webStateObserver.get());
  }
}

- (void)setOverlayPresenter:(OverlayPresenter*)overlayPresenter {
  if (_overlayPresenter) {
    _overlayPresenter->RemoveObserver(_overlayObserver.get());
  }

  _overlayPresenter = overlayPresenter;

  if (_overlayPresenter) {
    _overlayPresenter->AddObserver(_overlayObserver.get());
  }
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // The activation is handled after this switch statement.
      break;
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace:
      // Do nothing when a WebState is replaced.
      break;
    case WebStateListChange::Type::kInsert: {
      // For the external link open, the opened link can open in a new WebState.
      // Assume that is the case if a new WebState is inserted and activated
      // when the current web state is the one that was active when the link was
      // opened.
      if (self.currentPromoReason == PromoReasonExternalLink &&
          self.webStateList->GetActiveWebState() == self.webStateToListenTo &&
          status.active_web_state_change()) {
        const WebStateListChangeInsert& insertChange =
            change.As<WebStateListChangeInsert>();
        self.webStateToListenTo = insertChange.inserted_web_state();
      }
      break;
    }
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

  if (status.active_web_state_change()) {
    if (status.new_active_web_state != self.webStateToListenTo) {
      [self cancelShowPromoTimer];
    }
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  if (success && webState == self.webStateToListenTo) {
    self.webStateToListenTo = nil;
    [self startShowPromoTimer];
  }
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  [self cancelShowPromoTimer];
  [self dismissPromoAnimated:YES];
}

- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter {
  self.overlayPresenter = nullptr;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelBackground) {
    [self onEnteringBackground:self.currentPromoReason
                promoIsShowing:self.promoIsShowing];
  }
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  self.browser = nullptr;
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  self.browser =
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser;
  CHECK(self.browser);
}

#pragma mark - BrowserObserving

- (void)browserDestroyed:(Browser*)browser {
  self.browser = nullptr;
}

#pragma mark - Timer Management

// Start the timer to show a promo. `self.currentPromoReason` must be set to
// the reason for this promo flow and must not be `PromoReasonNone`.
- (void)startShowPromoTimer {
  DCHECK(self.currentPromoReason != PromoReasonNone);

  if (![self promoCanBeDisplayed]) {
    self.currentPromoReason = PromoReasonNone;
    self.webStateToListenTo = nullptr;
    return;
  }

  if (self.promoIsShowing || _showPromoTimer) {
    return;
  }

  base::TimeDelta promoTimeInterval;
  switch (self.currentPromoReason) {
    case PromoReasonNone:
      NOTREACHED_IN_MIGRATION();
      promoTimeInterval = kShowPromoWebpageLoadWaitTime;
      break;
    case PromoReasonOmniboxPaste:
      promoTimeInterval = kShowPromoWebpageLoadWaitTime;
      break;
    case PromoReasonExternalLink:
      promoTimeInterval = kShowPromoWebpageLoadWaitTime;
      break;
    case PromoReasonShare:
      promoTimeInterval = kShowPromoPostShareWaitTime;
      break;
  }

  __weak __typeof(self) weakSelf = self;
  _showPromoTimer = std::make_unique<base::OneShotTimer>();
  _showPromoTimer->Start(FROM_HERE, promoTimeInterval, base::BindOnce(^{
                           [weakSelf showPromoTimerFinished];
                         }));
}

- (void)cancelShowPromoTimer {
  // Only reset the reason and web state to listen to if there is no promo
  // showing.
  if (!self.promoIsShowing) {
    self.currentPromoReason = PromoReasonNone;
    self.webStateToListenTo = nullptr;
  }
  _showPromoTimer = nullptr;
}

- (void)showPromoTimerFinished {
  if (![self promoCanBeDisplayed] || self.promoIsShowing) {
    return;
  }
  _showPromoTimer = nullptr;
  [self notifyHandlerShowPromo];
  self.promoIsShowing = YES;
  [self logPromoAppear:self.currentPromoReason];
  self.promoShownTime = base::TimeTicks::Now();

  if (!UIAccessibilityIsVoiceOverRunning()) {
    [self startDismissPromoTimer];
  }
}

- (void)startDismissPromoTimer {
  if (_dismissPromoTimer) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _dismissPromoTimer = std::make_unique<base::OneShotTimer>();
  _dismissPromoTimer->Start(FROM_HERE, kPromoTimeout, base::BindOnce(^{
                              [weakSelf dismissPromoTimerFinished];
                            }));
}

- (void)dismissPromoTimerFinished {
  _dismissPromoTimer = nullptr;
  if (self.promoIsShowing) {
    [self logPromoTimeout:self.currentPromoReason
           promoShownTime:self.promoShownTime];
    self.promoShownTime = base::TimeTicks();
    [self notifyHandlerDismissPromo:YES];
  }
}

@end
