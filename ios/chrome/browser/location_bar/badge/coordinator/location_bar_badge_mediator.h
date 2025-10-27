// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/badges/ui_bundled/badge_view_visibility_delegate.h"
#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_view_visibility_delegate.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_mutator.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_visibility_delegate.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"

@protocol LocationBarBadgeConsumer;
@protocol LocationBarBadgeMediatorDelegate;

// Mediator for the location bar badge.
@interface LocationBarBadgeMediator
    : NSObject <BadgeViewVisibilityDelegate,
                IncognitoBadgeViewVisibilityDelegate,
                ReaderModeChipVisibilityDelegate,
                LocationBarBadgeCommands,
                LocationBarBadgeMutator>

// The consumer for this mediator.
@property(nonatomic, weak) id<LocationBarBadgeConsumer> consumer;
// The delegate for this mediator.
@property(nonatomic, weak) id<LocationBarBadgeMediatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_H_
