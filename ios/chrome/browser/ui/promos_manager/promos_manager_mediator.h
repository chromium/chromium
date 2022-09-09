// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <map>

#import "base/containers/small_map.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/ui/commands/promos_manager_commands.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_scene_availability_observer.h"

// A mediator that observes when it's a good time to display a promo, and
// communicates with PromosManager to find the next promo
// (promos_manager::Promo), if any, to display.
//
// If there exists a promo to display, communicates this information to the rest
// of the application via `handler`.
@interface PromosManagerMediator
    : NSObject <PromosManagerSceneAvailabilityObserver>

// Designated initializer.
- (instancetype)
    initWithPromosManager:(PromosManager*)promosManager
    promoImpressionLimits:
        (base::small_map<
            std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>)
            promoImpressionLimits
                  handler:(id<PromosManagerCommands>)handler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Records the display impression of `promo`.
- (void)recordImpression:(promos_manager::Promo)promo;

// The Promos Manager used for deciding which promo should be displayed, if any.
@property(nonatomic, assign) PromosManager* promosManager;

// Handler used to send a command for displaying a given promo.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_MEDIATOR_H_
