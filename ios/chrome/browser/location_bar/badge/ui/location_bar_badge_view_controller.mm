// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_view_controller.h"

#import "ios/chrome/browser/location_bar/ui_bundled/badges_container_view.h"

@implementation LocationBarBadgeViewController

#pragma mark - LocationBarBadgeConsumer

- (void)setFeature:(BadgeType)feature hidden:(BOOL)hidden {
  switch (feature) {
    case BadgeType::kNone:
      break;
    case BadgeType::kBadgeView:
      [self.badgesContainerView setBadgeViewHidden:hidden];
      break;
    case BadgeType::kIncognito:
      [self.badgesContainerView setIncognitoBadgeViewHidden:hidden];
      break;
    case BadgeType::kContextualPanel:
      [self.badgesContainerView setContextualPanelEntrypointHidden:hidden];
      break;
    case BadgeType::kReaderMode:
      // Reader chip coordinator isn't needed for setting visibility.
      [self.badgesContainerView readerModeChipCoordinator:nil
                               didSetReaderModeChipHidden:hidden];
      break;
  }
}

@end
