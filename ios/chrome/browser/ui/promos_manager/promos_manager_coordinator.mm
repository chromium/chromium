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
#import "ios/chrome/browser/ui/promos_manager/promos_manager_mediator.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_view_provider.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PromosManagerCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate> {
  // Promos that conform to the StandardPromoDisplayHandler protocol.
  base::small_map<
      std::map<promos_manager::Promo, id<StandardPromoDisplayHandler>>>
      _displayHandlerPromos;

  // Promos that conform to the StandardPromoViewProvider protocol.
  base::small_map<
      std::map<promos_manager::Promo, id<StandardPromoViewProvider>>>
      _viewProviderPromos;
}

// A mediator that observes when it's a good time to display a promo.
@property(nonatomic, strong) PromosManagerMediator* mediator;

// The current StandardPromoViewProvider, if any.
@property(nonatomic, weak) id<StandardPromoViewProvider> provider;

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

  DCHECK(handler_it == _displayHandlerPromos.end() ||
         provider_it == _viewProviderPromos.end());

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
  } else {
    NOTREACHED();
  }
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
  DCHECK(self.provider);

  if (![self.provider respondsToSelector:@selector(standardPromoDismissAction)])
    return;

  [self.provider standardPromoDismissAction];
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

  // StandardPromoViewProvider promo(s) below:
  if (post_restore_signin::features::CurrentPostRestoreSignInType() ==
      post_restore_signin::features::PostRestoreSignInType::kFullscreen)
    _viewProviderPromos[promos_manager::Promo::PostRestoreSignInFullscreen] =
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

  return result;
}

@end
