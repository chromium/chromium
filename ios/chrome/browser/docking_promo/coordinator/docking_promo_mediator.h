// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_COORDINATOR_DOCKING_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_COORDINATOR_DOCKING_PROMO_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

@protocol DockingPromoConsumer;

class PromosManager;

namespace feature_engagement {
class Tracker;
}

// Mediator that evaluates whether or not the Docking Promo can be displayed,
// and updates its consumer with the latest Docking Promo content.y
@interface DockingPromoMediator : NSObject

// The main consumer for this mediator.
@property(nonatomic, weak) id<DockingPromoConsumer> consumer;

// The feature engagement tracker to alert of promo events.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

// Designated initializer. Initializes the mediator with the PromosManager.
- (instancetype)initWithPromosManager:(PromosManager*)promosManager
              timeSinceLastForeground:(base::TimeDelta)timeSinceLastForeground
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithCoder NS_UNAVAILABLE;

// Returns YES if the user conditions are met to present the Docking Promo.
- (BOOL)canShowDockingPromo;

// Configures the consumer.
- (void)configureConsumer;

// Registers the Docking Promo (Remind Me Later version) for single display with
// the Promos Manager.
- (void)registerPromoWithPromosManager;

@end

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_COORDINATOR_DOCKING_PROMO_MEDIATOR_H_
