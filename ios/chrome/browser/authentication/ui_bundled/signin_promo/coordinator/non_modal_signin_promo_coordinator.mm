// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_coordinator.h"

#import "base/notreached.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_mediator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_metrics_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/signin_promo_types.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/non_modal_signin_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// The size of the logo image.
constexpr CGFloat kLogoSize = 22;
}  // namespace

@interface NonModalSignInPromoCoordinator () <
    NonModalSignInPromoMediatorDelegate>

@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;

@end

@implementation NonModalSignInPromoCoordinator {
  // The mediator responsible for the business logic.
  NonModalSignInPromoMediator* _mediator;
  // The type of promo to be displayed.
  SignInPromoType _promoType;
  raw_ptr<feature_engagement::Tracker> _tracker;
  // Whether the latest displayed infobar is not yet tapped.
  BOOL _infobarUntapped;
}

// Synthesize because readonly property from superclass is changed to readwrite.
@synthesize bannerViewController = _bannerViewController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 promoType:(SignInPromoType)promoType {
  self = [super initWithInfoBarDelegate:nil
                           badgeSupport:YES
                                   type:InfobarType::kInfobarTypeSignin];
  if (self) {
    self.baseViewController = viewController;
    self.browser = browser;
    self.shouldUseDefaultDismissal = NO;
    _promoType = promoType;
    _tracker = feature_engagement::TrackerFactory::GetForProfile(self.profile);
  }
  return self;
}

- (void)start {
  self.started = YES;
  // Create and configure the mediator
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);

  _mediator = [[NonModalSignInPromoMediator alloc]
      initWithAuthenticationService:authService
                    identityManager:identityManager
           featureEngagementTracker:_tracker
                          promoType:_promoType];

  _mediator.delegate = self;

  [_mediator startPromoDisplayTimer];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  // Clean up the banner if it's being displayed.
  [self hideBannerUI];
  [super stop];
}

#pragma mark - Private Methods

- (void)sendPromoDismissalNotification {
  if (self.bannerWasPresented) {
    switch (_promoType) {
      case SignInPromoType::kPassword:
        _tracker->Dismissed(
            feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature);
        break;
      case SignInPromoType::kBookmark:
        _tracker->Dismissed(
            feature_engagement::kIPHiOSPromoNonModalSigninBookmarkFeature);
        break;
    }
  }

  [_mediator stopTimeOutTimers];

  id<NonModalSignInPromoCommands> nonModalSignInPromoHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         NonModalSignInPromoCommands);
  [nonModalSignInPromoHandler dismissNonModalSignInPromo];
}

- (void)hideBannerUI {
  if (self.bannerViewController) {
    [self.bannerViewController dismissWhenInteractionIsFinished];
  }
}

- (void)configureBanner {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_NON_MODAL_SIGNIN_PROMO_PROMPT);
  NSString* buttonLabel =
      l10n_util::GetNSString(IDS_IOS_NON_MODAL_SIGNIN_PROMO_SIGNIN_BUTTON);

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* icon = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kMulticolorChromeballSymbol, kLogoSize));
#else
  UIImage* icon = CustomSymbolWithPointSize(kChromeProductSymbol, kLogoSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

  NSString* subtitle;

  // Configure subtitle based on promo type
  switch (_promoType) {
    case SignInPromoType::kPassword:
      subtitle =
          l10n_util::GetNSString(IDS_IOS_NON_MODAL_SIGNIN_PROMO_SAVE_PASSWORD);
      break;

    case SignInPromoType::kBookmark:
      subtitle =
          l10n_util::GetNSString(IDS_IOS_NON_MODAL_SIGNIN_PROMO_ADD_BOOKMARK);
      break;
  }

  // Configure the banner with the content
  [self.bannerViewController setTitleText:title];
  [self.bannerViewController setSubtitleText:subtitle];
  [self.bannerViewController setButtonText:buttonLabel];
  [self.bannerViewController setIconImage:icon];
  [self.bannerViewController setUseIconBackgroundTint:NO];
  [self.bannerViewController setPresentsModal:NO];
}

#pragma mark - InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  [self performInfobarAction];
}

#pragma mark - NonModalSignInPromoMediatorDelegate

- (void)nonModalSignInPromoMediatorTimerExpired:
    (NonModalSignInPromoMediator*)mediator {
  if (mediator != _mediator || self.bannerViewController) {
    return;
  }

  _infobarUntapped = YES;
  self.bannerViewController = [[InfobarBannerViewController alloc]
      initWithDelegate:self
         presentsModal:NO
                  type:InfobarType::kInfobarTypeSignin];

  [self configureBanner];

  // Record metrics for promo is appearing.
  LogNonModalSignInPromoAction(NonModalSignInPromoAction::kAppear, _promoType);

  [self presentInfobarBannerAnimated:YES completion:nil];
}

- (void)nonModalSignInPromoMediatorShouldDismiss:
    (NonModalSignInPromoMediator*)mediator {
  if (mediator != _mediator) {
    return;
  }

  [self sendPromoDismissalNotification];
}

#pragma mark - InfobarCoordinatorImplementation

- (BOOL)configureModalViewController {
  return NO;
}

- (BOOL)isInfobarAccepted {
  return NO;
}

- (BOOL)infobarBannerActionWillPresentModal {
  return NO;
}

- (void)infobarBannerWasPresented {
  // No-op.
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  // No-op.
}

- (void)dismissBannerIfReady {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

- (BOOL)infobarActionInProgress {
  return NO;
}

- (void)performInfobarAction {
  if (!_infobarUntapped) {
    // Double tap. Ignore
    return;
  }
  _infobarUntapped = NO;
  // Log sign-in action when user taps the sign-in button
  LogNonModalSignInPromoAction(NonModalSignInPromoAction::kAccept, _promoType);

  [self hideBannerUI];

  // Select the appropriate access point based on promo type
  signin_metrics::AccessPoint accessPoint;
  switch (_promoType) {
    case SignInPromoType::kPassword:
      accessPoint = signin_metrics::AccessPoint::kNonModalSigninPasswordPromo;
      break;
    case SignInPromoType::kBookmark:
      accessPoint = signin_metrics::AccessPoint::kNonModalSigninBookmarkPromo;
      break;
  }

  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  ShowSigninCommand* const showSigninCommand = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kSheetSigninAndHistorySync
               identity:nil
            accessPoint:accessPoint
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
             completion:nil];
  [handler showSignin:showSigninCommand
      baseViewController:self.baseViewController];
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  // Stop showing the promo if user manually dismissed it.
  if (userInitiated) {
    // Log user dismissal.
    LogNonModalSignInPromoAction(NonModalSignInPromoAction::kDismiss,
                                 _promoType);
  }
}

- (void)infobarWasDismissed {
  [self sendPromoDismissalNotification];
  self.bannerViewController = nil;
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  NOTREACHED();
}

@end
