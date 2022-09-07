// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator.h"

#import <Foundation/Foundation.h>
#import <map>

#import "base/containers/small_map.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/promos_manager_commands.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_mediator.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_scene_agent.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_view_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PromosManagerCoordinator () <PromosManagerCommands> {
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

@end

@implementation PromosManagerCoordinator

#pragma mark - Public

- (void)start {
  [self registerPromos];

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(PromosManagerCommands)];

  id<PromosManagerCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PromosManagerCommands);

  self.mediator = [[PromosManagerMediator alloc]
      initWithPromosManager:GetApplicationContext()->GetPromosManager()
      promoImpressionLimits:[self promoImpressionLimits]
                    handler:handler];

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();

  PromosManagerSceneAgent* sceneAgent =
      [PromosManagerSceneAgent agentFromScene:sceneState];

  [sceneAgent addObserver:self.mediator];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  self.mediator = nil;
}

#pragma mark - PromosManagerCommands

- (void)displayPromo:(promos_manager::Promo)promo {
  // TODO(crbug.com/1358991):
  // 1. Grab the proper view provider or display handler that's registered with
  // the coordinator
  // 2. Call the proper view provider or display handler.
  // 3. Let the mediator know that `promo` was displayed.
}

#pragma mark - Private

- (void)registerPromos {
  // Add StandardPromoDisplayHandler promos here. For example:
  // TODO(crbug.com/1360880): Create first StandardPromoDisplayHandler promo.

  // Add StandardPromoViewProvider promos here. For example:
  // TODO(crbug.com/1360881): Create first StandardPromoViewProvider promo.
}

- (base::small_map<std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>)
    promoImpressionLimits {
  base::small_map<std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>
      result;

  for (auto const& [promo, handler] : _displayHandlerPromos)
    result[promo] = handler.impressionLimits;

  for (auto const& [promo, provider] : _viewProviderPromos)
    result[promo] = provider.impressionLimits;

  return result;
}

@end
