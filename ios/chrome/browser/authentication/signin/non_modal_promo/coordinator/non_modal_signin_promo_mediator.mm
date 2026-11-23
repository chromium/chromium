// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_mediator.h"

#import <memory>
#import <optional>
#import <string>

#import "base/callback_list.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/timer/timer.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_metrics_util.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_types.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/model/non_modal_signin_promo_infobar_delegate.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_dispatch_callback.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Delay before showing the promo after the timer starts.
constexpr base::TimeDelta kPromoDisplayDelay = base::Seconds(1);

// Returns whether it is possible to start a sign-in.
bool SigninIsPossible(AuthenticationService* auth_service) {
  return !auth_service->HasPrimaryIdentity() && auth_service->SigninEnabled();
}

// Returns whether `infobar` is the promo infobar.
bool IsPromoInfobar(infobars::InfoBar* infobar) {
  return infobar->delegate()->GetIdentifier() ==
         infobars::InfoBarDelegate::NON_MODAL_SIGNIN_PROMO_INFOBAR_DELEGATE_IOS;
}

// Returns whether `request` is the banner request of the promo infobar.
bool IsPromoInfobarRequest(OverlayRequest* request) {
  InfobarOverlayRequestConfig* config =
      request->GetConfig<InfobarOverlayRequestConfig>();
  return config && config->infobar() && IsPromoInfobar(config->infobar());
}

// Returns the feature engagement feature for `promo_type`.
const base::Feature& FeatureForPromoType(NonModalSignInPromoType promo_type) {
  switch (promo_type) {
    case NonModalSignInPromoType::kPassword:
      return feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature;
    case NonModalSignInPromoType::kBookmark:
      return feature_engagement::kIPHiOSPromoNonModalSigninBookmarkFeature;
  }
}

// Returns the banner message string for `promo_type`.
std::u16string MessageTextForPromoType(NonModalSignInPromoType promo_type) {
  switch (promo_type) {
    case NonModalSignInPromoType::kPassword:
      return l10n_util::GetStringUTF16(
          IDS_IOS_NON_MODAL_SIGNIN_PROMO_SAVE_PASSWORD);
    case NonModalSignInPromoType::kBookmark:
      return l10n_util::GetStringUTF16(
          IDS_IOS_NON_MODAL_SIGNIN_PROMO_ADD_BOOKMARK);
  }
}

// Observes the InfoBarManager of the WebState showing the promo. Banners are
// presented one at a time, so another banner added to that WebState would wait
// behind the promo; the promo is dismissed instead.
class PromoInfoBarManagerObserver : public infobars::InfoBarManager::Observer {
 public:
  PromoInfoBarManagerObserver(infobars::InfoBarManager* manager,
                              base::RepeatingClosure banner_added_callback)
      : banner_added_callback_(std::move(banner_added_callback)) {
    observation_.Observe(manager);
  }

  // infobars::InfoBarManager::Observer:
  void OnInfoBarAdded(infobars::InfoBar* infobar) override {
    if (IsPromoInfobar(infobar) ||
        static_cast<InfoBarIOS*>(infobar)->skip_banner()) {
      return;
    }
    // Running the callback may delete `this`, so members must not be accessed
    // afterwards.
    base::RepeatingClosure callback = banner_added_callback_;
    callback.Run();
  }

  void OnManagerWillBeDestroyed(infobars::InfoBarManager* manager) override {
    observation_.Reset();
  }

 private:
  base::RepeatingClosure banner_added_callback_;
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      observation_{this};
};

}  // namespace

@interface NonModalSignInPromoMediator () <AuthenticationServiceObserving,
                                           IdentityManagerObserving,
                                           OverlayPresenterObserving,
                                           WebStateListObserving>
@end

@implementation NonModalSignInPromoMediator {
  // Bridge to observe changes in the identity manager. Set in `-init...` and
  // reset in `-disconnect`.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // Observer for auth service status changes. Set in `-init...` and reset in
  // `-disconnect`.
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;

  // Timer to delay displaying the promo. Set in `-startPromoDisplayTimer` and
  // reset when it fires, is cancelled, or in `-disconnect`.
  std::unique_ptr<base::OneShotTimer> _displayTimer;

  // The AuthenticationService used by the mediator to monitor sign-in status.
  // Cleared in `-disconnect`.
  raw_ptr<AuthenticationService> _authService;

  // Feature engagement tracker for determining if the promo can be shown.
  // Cleared in `-disconnect`.
  raw_ptr<feature_engagement::Tracker> _tracker;

  // The browser's WebStateList, used to add the promo infobar and detect active
  // tab changes. Cleared in `-disconnect` or `-webStateListDestroyed:`.
  raw_ptr<WebStateList> _webStateList;

  // Bridge and scoped observation for `_webStateList`. Set in `-init...` and
  // reset in `-disconnect` or `-webStateListDestroyed:`.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::optional<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _webStateListObservation;

  // Presenter of the browser's infobar banners. Cleared in `-disconnect` or
  // `-overlayPresenterDestroyed:`.
  raw_ptr<OverlayPresenter> _overlayPresenter;

  // Bridge and scoped observation for `_overlayPresenter`. Set in `-init...`
  // and reset in `-disconnect` or `-overlayPresenterDestroyed:`.
  std::unique_ptr<OverlayPresenterObserverBridge> _overlayPresenterObserver;
  std::optional<
      base::ScopedObservation<OverlayPresenter, OverlayPresenterObserver>>
      _overlayPresenterObservation;

  // Subscription observing browser view visibility changes to dismiss the promo
  // when the browser view leaves the hierarchy. Set in `-init...` (when a
  // visibility notifier is provided) and reset in `-disconnect`.
  base::CallbackListSubscription _browserViewVisibilitySubscription;

  // The infobar backing the promo banner, if it is showing. Set in
  // `-addPromoInfobar` and reset in `-dismissPromoInfobar` or
  // `-promoInfobarDestroyed`.
  raw_ptr<InfoBarIOS> _promoInfobar;

  // Observer of the InfoBarManager owning `_promoInfobar`. Set when the promo
  // infobar is added and reset when it is dismissed or destroyed.
  std::unique_ptr<PromoInfoBarManagerObserver> _promoInfobarManagerObserver;

  // The type of promo (password or bookmark) being shown.
  NonModalSignInPromoType _promoType;

  // Whether the feature engagement tracker counted the promo as shown, so it
  // must be told when the promo is over.
  BOOL _promoTriggered;

  // Whether the banner was presented without any outcome recorded yet, so that
  // its removal counts as a timeout.
  BOOL _timeoutPending;

  // Whether `-disconnect` has been called.
  BOOL _disconnected;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager
         featureEngagementTracker:(feature_engagement::Tracker*)tracker
                     webStateList:(WebStateList*)webStateList
                 overlayPresenter:(OverlayPresenter*)overlayPresenter
    browserViewVisibilityNotifier:
        (BrowserViewVisibilityNotifierBrowserAgent*)visibilityNotifier
                        promoType:(NonModalSignInPromoType)promoType {
  self = [super init];
  if (self) {
    CHECK(authService);
    CHECK(identityManager);
    CHECK(tracker);
    CHECK(webStateList);
    CHECK(overlayPresenter);
    _authService = authService;
    _tracker = tracker;
    _webStateList = webStateList;
    _overlayPresenter = overlayPresenter;
    _promoType = promoType;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _authServiceObserverBridge =
        std::make_unique<AuthenticationServiceObserverBridge>(authService,
                                                              self);

    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateListObservation.emplace(_webStateListObserver.get());
    _webStateListObservation->Observe(_webStateList);

    _overlayPresenterObserver =
        std::make_unique<OverlayPresenterObserverBridge>(self);
    _overlayPresenterObservation.emplace(_overlayPresenterObserver.get());
    _overlayPresenterObservation->Observe(_overlayPresenter);

    if (visibilityNotifier) {
      __weak __typeof(self) weakSelf = self;
      _browserViewVisibilitySubscription =
          visibilityNotifier->RegisterBrowserVisibilityStateChangedCallback(
              base::BindRepeating(^(BrowserViewVisibilityState currentState,
                                    BrowserViewVisibilityState previousState) {
                [weakSelf browserViewVisibilityDidChange:currentState];
              }));
    }
  }
  return self;
}

- (void)dealloc {
  CHECK(_disconnected);
}

#pragma mark - Public

- (void)startPromoDisplayTimer {
  _displayTimer = nullptr;
  if (!SigninIsPossible(_authService) ||
      !_tracker->WouldTriggerHelpUI(FeatureForPromoType(_promoType))) {
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _displayTimer = std::make_unique<base::OneShotTimer>();
  _displayTimer->Start(FROM_HERE, kPromoDisplayDelay, base::BindOnce(^{
                         [weakSelf displayTimerFired];
                       }));
}

- (void)disconnect {
  if (_disconnected) {
    return;
  }
  _disconnected = YES;
  _displayTimer = nullptr;
  _browserViewVisibilitySubscription = {};
  _webStateListObservation.reset();
  _webStateListObserver.reset();
  _overlayPresenterObservation.reset();
  _overlayPresenterObserver.reset();
  _identityManagerObserver.reset();
  _authServiceObserverBridge.reset();
  [self dismissPromoInfobar];
  [self notifyTrackerDismissedIfNeeded];
  _webStateList = nullptr;
  _overlayPresenter = nullptr;
  _authService = nullptr;
  _tracker = nullptr;
}

#pragma mark - IdentityManagerObserving

- (void)primaryAccountDidChange:
    (const signin::PrimaryAccountChangeEvent&)event {
  [self maybeCancelPromo];
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  [self maybeCancelPromo];
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  if (!IsPromoInfobarRequest(request)) {
    // Another banner is shown instead of the promo.
    [self dismissPromoInfobar];
    return;
  }
  if (!initialPresentation) {
    return;
  }
  _timeoutPending = YES;
  LogNonModalSignInPromoAction(NonModalSignInPromoAction::kAppear, _promoType);
  [self observeUserDismissalForRequest:request];
}

- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter {
  _overlayPresenterObservation.reset();
  _overlayPresenterObserver.reset();
  _overlayPresenter = nullptr;
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  // The promo banner belongs to the previously active WebState.
  if (status.active_web_state_change()) {
    [self dismissPromoInfobar];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  _webStateListObservation.reset();
  _webStateListObserver.reset();
  _webStateList = nullptr;
}

#pragma mark - Private

// Called when the display timer fires.
- (void)displayTimerFired {
  _displayTimer = nullptr;

  // Don't queue the promo behind another banner that is already presented, and
  // only display when the browser view is on screen.
  if ((_overlayPresenter && _overlayPresenter->IsShowingOverlayUI()) ||
      ![self.delegate nonModalSignInPromoMediatorCanDisplayPromo:self]) {
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
    return;
  }

  if (!_tracker->ShouldTriggerHelpUI(FeatureForPromoType(_promoType))) {
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
    return;
  }

  _promoTriggered = YES;
  [self addPromoInfobar];
}

// Adds the promo infobar to the active WebState, which presents the promo
// banner. Notifies the delegate to dismiss if the infobar cannot be added.
- (void)addPromoInfobar {
  web::WebState* activeWebState =
      _webStateList ? _webStateList->GetActiveWebState() : nullptr;
  if (!activeWebState) {
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
    return;
  }
  InfoBarManagerImpl* infobarManager =
      InfoBarManagerImpl::FromWebState(activeWebState);
  if (!infobarManager) {
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  auto delegate = std::make_unique<NonModalSignInPromoInfoBarDelegate>(
      MessageTextForPromoType(_promoType), base::BindOnce(^{
        [weakSelf promoAccepted];
      }),
      base::BindOnce(^{
        [weakSelf promoInfobarDestroyed];
      }));
  // If the infobar can't be added, it is destroyed right away, which ends the
  // promo through `-promoInfobarDestroyed`.
  infobars::InfoBar* infobar =
      infobarManager->AddInfoBar(std::make_unique<InfoBarIOS>(
          InfobarType::kInfobarTypeConfirm, std::move(delegate)));
  if (!infobar) {
    return;
  }
  _promoInfobar = static_cast<InfoBarIOS*>(infobar);
  _promoInfobarManagerObserver = std::make_unique<PromoInfoBarManagerObserver>(
      infobarManager, base::BindRepeating(^{
        [weakSelf dismissPromoInfobar];
      }));
}

// Dismisses the promo infobar, if any, without recording it as a timeout.
- (void)dismissPromoInfobar {
  InfoBarIOS* infobar = _promoInfobar.get();
  _promoInfobar = nullptr;
  _promoInfobarManagerObserver = nullptr;
  if (!infobar) {
    return;
  }
  _timeoutPending = NO;
  // iOS infobars are deleted synchronously when removed, which calls
  // `-promoInfobarDestroyed`.
  CHECK(infobar->owner());
  infobar->owner()->RemoveInfoBar(infobar);
}

// Called when the promo infobar is destroyed, whatever the reason.
- (void)promoInfobarDestroyed {
  _promoInfobar = nullptr;
  _promoInfobarManagerObserver = nullptr;
  if (_timeoutPending) {
    _timeoutPending = NO;
    LogNonModalSignInPromoAction(NonModalSignInPromoAction::kTimeout,
                                 _promoType);
  }
  [self notifyTrackerDismissedIfNeeded];
  if (!_disconnected) {
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
  }
}

// Notifies the feature engagement tracker that the promo is over, if it was
// triggered and hasn't been reported as dismissed yet.
- (void)notifyTrackerDismissedIfNeeded {
  if (!_promoTriggered) {
    return;
  }
  _promoTriggered = NO;
  _tracker->Dismissed(FeatureForPromoType(_promoType));
}

// Handles browser view visibility changes.
- (void)browserViewVisibilityDidChange:(BrowserViewVisibilityState)state {
  // The banner can only be seen on the browser view, e.g. not in the tab grid.
  if (state == BrowserViewVisibilityState::kNotInViewHierarchy) {
    [self dismissPromoInfobar];
  }
}

// Records the user-initiated dismissals of the promo banner shown for
// `request`.
- (void)observeUserDismissalForRequest:(OverlayRequest*)request {
  __weak __typeof(self) weakSelf = self;
  request->GetCallbackManager()->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(^(OverlayResponse* response) {
        [weakSelf userDismissedPromo];
      }),
      InfobarBannerUserInitiatedDismissalResponse::ResponseSupport()));
}

// Handles the user manually dismissing the promo banner.
- (void)userDismissedPromo {
  _timeoutPending = NO;
  LogNonModalSignInPromoAction(NonModalSignInPromoAction::kDismiss, _promoType);
}

// Handles the user tapping the promo banner's sign-in button.
- (void)promoAccepted {
  if (_disconnected) {
    return;
  }
  _timeoutPending = NO;
  if (!SigninIsPossible(_authService)) {
    // The promo is not scheduled if the user is signed-in or if sign-in is
    // disabled. Still, due to asynchronicity, the state could have changed in
    // the meantime, so it needs to be checked again before starting sign-in.
    return;
  }
  LogNonModalSignInPromoAction(NonModalSignInPromoAction::kAccept, _promoType);
  [self.delegate nonModalSignInPromoMediatorDidAccept:self];
}

// Cancels the promo if sign-in is no longer possible.
- (void)maybeCancelPromo {
  if (SigninIsPossible(_authService)) {
    return;
  }
  _displayTimer = nullptr;
  if (_promoInfobar) {
    // Ends the promo through `-promoInfobarDestroyed`.
    [self dismissPromoInfobar];
    return;
  }
  [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
}

@end
