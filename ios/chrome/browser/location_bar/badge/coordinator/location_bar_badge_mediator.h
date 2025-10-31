// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_mutator.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"

@protocol BWGCommands;
@protocol LocationBarBadgeConsumer;
@protocol LocationBarBadgeMediatorDelegate;
class PrefService;
class WebStateList;
class BwgService;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

// Mediator for the location bar badge.
@interface LocationBarBadgeMediator
    : NSObject <LocationBarBadgeCommands, LocationBarBadgeMutator>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                             tracker:(feature_engagement::Tracker*)tracker
                         prefService:(PrefService*)prefService
                       geminiService:(BwgService*)geminiService

    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer for this mediator.
@property(nonatomic, weak) id<LocationBarBadgeConsumer> consumer;
// The delegate for this mediator.
@property(nonatomic, weak) id<LocationBarBadgeMediatorDelegate> delegate;
// The command handler for Gemini commands.
@property(nonatomic, weak) id<BWGCommands> BWGCommandHandler;

// Cleans up mediator properties and variables.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_H_
