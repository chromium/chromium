// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator.h"

#import <Foundation/Foundation.h>
#import <map>

#import "base/check.h"
#import "base/containers/small_map.h"
#import "base/notreached.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"
#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"
#import "ios/chrome/browser/ui/promos_manager/bannered_promo_view_provider.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_mediator.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_view_provider.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PromosManagerCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate,
    PromoStyleViewControllerDelegate> {
  // Promos that conform to the StandardPromoDisplayHandler protocol.
  base::small_map<
      std::map<promos_manager::Promo, id<StandardPromoDisplayHandler>>>
      _displayHandlerPromos;

  // Promos that conform to the StandardPromoViewProvider protocol.
  base::small_map<
      std::map<promos_manager::Promo, id<StandardPromoViewProvider>>>
      _viewProviderPromos;

  // Promos that conform to the BanneredPromoViewProvider protocol.
  base::small_map<
      std::map<promos_manager::Promo, id<BanneredPromoViewProvider>>>
      _banneredViewProviderPromos;
}

// A mediator that observes when it's a good time to display a promo.
@property(nonatomic, strong) PromosManagerMediator* mediator;

// The current StandardPromoViewProvider, if any.
@property(nonatomic, weak) id<StandardPromoViewProvider> provider;

// The current BanneredPromoViewProvider, if any.
@property(nonatomic, weak) id<BanneredPromoViewProvider> banneredProvider;

@end

@implementation PromosManagerCoordinator

#pragma mark - Public

- (void)start {
  [self registerPromos];

  self.mediator = [[PromosManagerMediator alloc]
      initWithPromosManager:GetApplicationContext()->GetPromosManager()
      promoImpressionLimits:[self promoImpressionLimits]];

  absl::optional<promos_manager::Promo> nextPromoForDisplay =
      [self.mediator nextPromoForDisplay];

  if (nextPromoForDisplay.has_value())
    [self displayPromo:nextPromoForDisplay.value()];
}

- (void)stop {
  self.mediator = nil;
}

- (void)displayPromo:(promos_manager::Promo)promo {
  auto handler_it = _displayHandlerPromos.find(promo);
  auto provider_it = _viewProviderPromos.find(promo);
  auto bannered_provider_it = _banneredViewProviderPromos.find(promo);

  DCHECK(handler_it == _displayHandlerPromos.end() ||
         provider_it == _viewProviderPromos.end() ||
         bannered_provider_it == _banneredViewProviderPromos.end());

  if (handler_it != _displayHandlerPromos.end()) {
    id<StandardPromoDisplayHandler> handler = handler_it->second;

    [handler handleDisplay];

    [self.mediator recordImpression:handler.identifier];
  } else if (provider_it != _viewProviderPromos.end()) {
    id<StandardPromoViewProvider> provider = provider_it->second;

    ConfirmationAlertViewController* promoViewController =
        [provider viewController];
    promoViewController.presentationController.delegate = self;
    promoViewController.actionHandler = self;
    self.provider = provider;

    [self.baseViewController presentViewController:promoViewController
                                          animated:YES
                                        completion:nil];

    [self.mediator recordImpression:provider.identifier];
  } else if (bannered_provider_it != _banneredViewProviderPromos.end()) {
    id<BanneredPromoViewProvider> banneredProvider =
        bannered_provider_it->second;

    PromoStyleViewController* promoViewController =
        [banneredProvider viewController];

    promoViewController.presentationController.delegate = self;
    promoViewController.delegate = self;
    self.banneredProvider = banneredProvider;

    [self.baseViewController presentViewController:promoViewController
                                          animated:YES
                                        completion:nil];

    [self.mediator recordImpression:banneredProvider.identifier];
  } else {
    NOTREACHED();
  }
}

#pragma mark - PromoStyleViewControllerDelegate

// Invoked when the primary action button is tapped.
- (void)didTapPrimaryActionButton {
  DCHECK(self.banneredProvider);

  if (![self.banneredProvider
          respondsToSelector:@selector(standardPromoPrimaryAction)])
    return;

  [self.banneredProvider standardPromoPrimaryAction];
}

// Invoked when the secondary action button is tapped.
- (void)didTapSecondaryActionButton {
  DCHECK(self.banneredProvider);

  if (![self.banneredProvider
          respondsToSelector:@selector(standardPromoSecondaryAction)])
    return;

  [self.banneredProvider standardPromoSecondaryAction];
}

// Invoked when the tertiary action button is tapped.
- (void)didTapTertiaryActionButton {
  DCHECK(self.banneredProvider);

  if (![self.banneredProvider
          respondsToSelector:@selector(standardPromoTertiaryAction)])
    return;

  [self.banneredProvider standardPromoTertiaryAction];
}

// Invoked when the top left question mark button is tapped.
- (void)didTapLearnMoreButton {
  DCHECK(self.banneredProvider);

  if (![self.banneredProvider
          respondsToSelector:@selector(standardPromoLearnMoreAction)])
    return;

  [self.banneredProvider standardPromoLearnMoreAction];
}

// Invoked when a link in the disclaimer is tapped.
- (void)didTapURLInDisclaimer:(NSURL*)URL {
  // TODO(crbug.com/1363906): Complete `didTapURLInDisclaimer` to bring users to
  // Settings page.
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  DCHECK(self.provider);

  if (![self.provider respondsToSelector:@selector(standardPromoPrimaryAction)])
    return;

  [self.provider standardPromoPrimaryAction];
}

- (void)confirmationAlertSecondaryAction {
  DCHECK(self.provider);

  if (![self.provider
          respondsToSelector:@selector(standardPromoSecondaryAction)])
    return;

  [self.provider standardPromoSecondaryAction];
}

- (void)confirmationAlertTertiaryAction {
  DCHECK(self.provider);

  if (![self.provider
          respondsToSelector:@selector(standardPromoTertiaryAction)])
    return;

  [self.provider standardPromoTertiaryAction];
}

- (void)confirmationAlertLearnMoreAction {
  DCHECK(self.provider);

  if (![self.provider
          respondsToSelector:@selector(standardPromoLearnMoreAction)])
    return;

  [self.provider standardPromoLearnMoreAction];
}

- (void)confirmationAlertDismissAction {
  DCHECK(self.provider || self.banneredProvider);

  if ([self.provider
          respondsToSelector:@selector(standardPromoDismissAction)]) {
    [self.provider standardPromoDismissAction];
  } else if ([self.banneredProvider
                 respondsToSelector:@selector(standardPromoDismissAction)]) {
    [self.banneredProvider standardPromoDismissAction];
  } else {
    NOTREACHED();
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self confirmationAlertDismissAction];
}

#pragma mark - Private

- (void)registerPromos {
  // Add StandardPromoDisplayHandler promos here. For example:
  // TODO(crbug.com/1360880): Create first StandardPromoDisplayHandler promo.

  // Add StandardPromoViewProvider promos here. For example:
  // TODO(crbug.com/1360880): Create first StandardPromoViewProvider promo.

  // BanneredPromoViewProvider promo(s) below:
  if (post_restore_signin::features::CurrentPostRestoreSignInType() ==
      post_restore_signin::features::PostRestoreSignInType::kFullscreen)
    _banneredViewProviderPromos
        [promos_manager::Promo::PostRestoreSignInFullscreen] =
            [[PostRestoreSignInProvider alloc] init];
}

- (base::small_map<std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>)
    promoImpressionLimits {
  base::small_map<std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>
      result;

  for (auto const& [promo, handler] : _displayHandlerPromos)
    if ([handler respondsToSelector:@selector(impressionLimits)])
      result[promo] = handler.impressionLimits;

  for (auto const& [promo, provider] : _viewProviderPromos)
    if ([provider respondsToSelector:@selector(impressionLimits)])
      result[promo] = provider.impressionLimits;

  for (auto const& [promo, banneredProvider] : _banneredViewProviderPromos)
    if ([banneredProvider respondsToSelector:@selector(impressionLimits)])
      result[promo] = banneredProvider.impressionLimits;

  return result;
}

@end
