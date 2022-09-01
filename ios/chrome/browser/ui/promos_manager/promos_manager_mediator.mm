// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_mediator.h"

#import "ios/chrome/browser/promos_manager/constants.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PromosManagerMediator

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                              handler:(id<PromosManagerCommands>)handler {
  if (self = [super init]) {
    _promosManager = promosManager;
    _handler = handler;
  }

  return self;
}

#pragma mark - PromosManagerSceneAvailabilityObserver

// Queries the PromosManager for the next promo (promos_manager::Promo) to
// display, if any.
//
// If there's an eligible promo to display, dispatches it via `handler` to be
// handled by the rest of the application.
- (void)sceneDidBecomeAvailableForPromo {
  DCHECK_NE(_promosManager, nullptr);
  DCHECK(_handler);

  absl::optional<promos_manager::Promo> nextPromoForDisplay =
      self.promosManager->NextPromoForDisplay();

  if (nextPromoForDisplay.has_value())
    [self.handler displayPromo:nextPromoForDisplay.value()];
}

@end
