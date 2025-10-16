// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/badges/ui_bundled/badge_view_visibility_delegate.h"
#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_view_visibility_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_visibility_delegate.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_visibility_delegate.h"

@protocol LocationBarBadgeConsumer;

// Mediator for the location bar badge.
// TODO(crbug.com/445719031): Implement this.
@interface LocationBarBadgeMediator
    : NSObject <BadgeViewVisibilityDelegate,
                ContextualPanelEntrypointVisibilityDelegate,
                IncognitoBadgeViewVisibilityDelegate,
                ReaderModeChipVisibilityDelegate>

@property(nonatomic, weak) id<LocationBarBadgeConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_MEDIATOR_H_
