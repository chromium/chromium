// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui/badge/location_bar_badge_mediator.h"

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_chip_coordinator.h"

@implementation LocationBarBadgeMediator

#pragma mark - BadgeViewVisibilityDelegate

- (void)setBadgeViewHidden:(BOOL)hidden {
  // TODO(crbug.com/446372417): Send to view through a consumer.
}

#pragma mark - ContextualPanelEntrypointVisibilityDelegate

- (void)setContextualPanelEntrypointHidden:(BOOL)hidden {
  // TODO(crbug.com/446372417): Send to view through a consumer.
}

#pragma mark - IncognitoBadgeViewVisibilityDelegate

- (void)setIncognitoBadgeViewHidden:(BOOL)hidden {
  // TODO(crbug.com/446372417): Send to view through a consumer.
}

#pragma mark - ReaderModeChipVisibilityDelegate

- (void)readerModeChipCoordinator:(ReaderModeChipCoordinator*)coordinator
       didSetReaderModeChipHidden:(BOOL)hidden {
  // TODO(crbug.com/446372417): Send to view through a consumer.
}

@end
