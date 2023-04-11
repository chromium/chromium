// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_scheduler.h"

#import "base/time/time.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_observer_bridge.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_metrics_util.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Default time interval to wait to show the promo after loading a webpage.
// This should allow any initial overlays to be presented first.
constexpr base::TimeDelta kShowPromoWebpageLoadWaitTime = base::Seconds(3);

// Default time interval to wait to show the promo after the share action is
// completed.
constexpr base::TimeDelta kShowPromoPostShareWaitTime = base::Seconds(1);

// Number of times to show the promo to a user.
const int kPromoShownTimesLimit = 3;

// Timeout before the promo is dismissed.
constexpr base::TimeDelta kPromoTimeout = base::Seconds(45);

bool PromoCanBeDisplayed() {
  if (!AreDefaultBrowserPromosEnabled()) {
    return false;
  }

  if (IsChromeLikelyDefaultBrowser())
    return false;

  if (UserInPromoCooldown())
    return false;

  NSInteger count = UserInteractionWithNonModalPromoCount();
  return count < kPromoShownTimesLimit;
}

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
      NOTREACHED();
      break;
  }
}

}  // namespace

@interface DefaultBrowserPromoNonModalScheduler () <WebStateListObserving,
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
}

// Time when a non modal promo was shown on screen, used for metrics only.
@property(nonatomic) base::TimeTicks promoShownTime;

// WebState that the triggering event occured in.
@property(nonatomic, assign) web::WebState* webStateToListenTo;

// The handler used to respond to the promo show/hide commands.
@property(nonatomic, readonly) id<DefaultBrowserPromoNonModalCommands> handler;

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

@end

@implementation DefaultBrowserPromoNonModalScheduler

- (instancetype)init {
  if (self = [super init]) {
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
  self.currentPromoReason = PromoReasonShare;
  [self startShowPromoTimer];
}

- (void)logUserEnteredAppViaFirstPartyScheme {
  if (self.currentPromoReason != PromoReasonNone) {
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
  LogNonModalPromoAction(NonModalPromoAction::kAccepted,
                         MetricTypeForPromoReason(self.currentPromoReason),
                         UserInteractionWithNonModalPromoCount());
  LogNonModalTimeOnScreen(self.promoShownTime);
  self.promoShownTime = base::TimeTicks();
  LogUserInteractionWithNonModalPromo();

  NSURL* settingsURL = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
  [[UIApplication sharedApplication] openURL:settingsURL
                                     options:@{}
                           completionHandler:nil];
}

- (void)logUserDismissedPromo {
  LogNonModalPromoAction(NonModalPromoAction::kDismiss,
                         MetricTypeForPromoReason(self.currentPromoReason),
                         UserInteractionWithNonModalPromoCount());
  LogNonModalTimeOnScreen(self.promoShownTime);
  self.promoShownTime = base::TimeTicks();
  LogUserInteractionWithNonModalPromo();
}

- (void)dismissPromoAnimated:(BOOL)animated {
  [self cancelDismissPromoTimer];
  [self.handler dismissDefaultBrowserNonModalPromoAnimated:animated];
}

- (void)setBrowser:(Browser*)browser {
  if (_browser) {
    self.webStateList = nullptr;
    self.overlayPresenter = nullptr;
  }

  _browser = browser;

  if (_browser) {
    _browserObserver = std::make_unique<BrowserObserverBridge>(_browser, self);
    self.webStateList = _browser->GetWebStateList();
    self.overlayPresenter = OverlayPresenter::FromBrowser(
        _browser, OverlayModality::kInfobarBanner);
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

- (id<DefaultBrowserPromoNonModalCommands>)handler {
  return HandlerForProtocol(self.dispatcher,
                            DefaultBrowserPromoNonModalCommands);
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  if (newWebState != self.webStateToListenTo) {
    [self cancelShowPromoTimer];
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  // For the external link open, the opened link can open in a new webstate.
  // Assume that is the case if a new WebState is inserted and activated when
  // the current web state is the one that was active when the link was opened.
  if (self.currentPromoReason == PromoReasonExternalLink &&
      self.webStateList->GetActiveWebState() == self.webStateToListenTo &&
      activating) {
    self.webStateToListenTo = webState;
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

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelBackground) {
    if (self.currentPromoReason != PromoReasonNone && !self.promoIsShowing) {
      LogNonModalPromoAction(NonModalPromoAction::kBackgroundCancel,
                             MetricTypeForPromoReason(self.currentPromoReason),
                             UserInteractionWithNonModalPromoCount());
    }
    [self cancelShowPromoTimer];
    [self cancelDismissPromoTimer];
    [self.handler dismissDefaultBrowserNonModalPromoAnimated:NO];
  }
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

  if (!PromoCanBeDisplayed()) {
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
      NOTREACHED();
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
  if (!PromoCanBeDisplayed() || self.promoIsShowing) {
    return;
  }
  _showPromoTimer = nullptr;
  [self.handler showDefaultBrowserNonModalPromo];
  self.promoIsShowing = YES;
  LogNonModalPromoAction(NonModalPromoAction::kAppear,
                         MetricTypeForPromoReason(self.currentPromoReason),
                         UserInteractionWithNonModalPromoCount());
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

- (void)cancelDismissPromoTimer {
  _dismissPromoTimer = nullptr;
}

- (void)dismissPromoTimerFinished {
  _dismissPromoTimer = nullptr;
  if (self.promoIsShowing) {
    LogNonModalPromoAction(NonModalPromoAction::kTimeout,
                           MetricTypeForPromoReason(self.currentPromoReason),
                           UserInteractionWithNonModalPromoCount());
    LogNonModalTimeOnScreen(self.promoShownTime);
    self.promoShownTime = base::TimeTicks();
    LogUserInteractionWithNonModalPromo();
    [self.handler dismissDefaultBrowserNonModalPromoAnimated:YES];
  }
}

@end
