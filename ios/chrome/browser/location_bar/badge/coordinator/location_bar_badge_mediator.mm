// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_consumer.h"

@implementation LocationBarBadgeMediator

#pragma mark - BadgeViewVisibilityDelegate

- (void)setBadgeViewHidden:(BOOL)hidden {
  [self.consumer setFeature:BadgeType::kBadgeView hidden:hidden];
}

#pragma mark - ContextualPanelEntrypointVisibilityDelegate

- (void)setContextualPanelEntrypointHidden:(BOOL)hidden {
  [self.consumer setFeature:BadgeType::kContextualPanel hidden:hidden];
}

#pragma mark - IncognitoBadgeViewVisibilityDelegate

- (void)setIncognitoBadgeViewHidden:(BOOL)hidden {
  [self.consumer setFeature:BadgeType::kIncognito hidden:hidden];
}

#pragma mark - ReaderModeChipVisibilityDelegate

- (void)readerModeChipCoordinator:(ReaderModeChipCoordinator*)coordinator
       didSetReaderModeChipHidden:(BOOL)hidden {
  [self.consumer setFeature:BadgeType::kReaderMode hidden:hidden];
}

@end
