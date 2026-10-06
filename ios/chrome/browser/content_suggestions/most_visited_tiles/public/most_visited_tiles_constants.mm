// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/public/most_visited_tiles_constants.h"

#import "components/ntp_tiles/features.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"

namespace {

constexpr CGFloat kMostVisitedIconTitleSpacing = 10.0;
constexpr CGFloat kMostVisitedIconTitleSpacingUICleanup = 8.0;
constexpr CGFloat kMostVisitedIconTitleSpacingWithoutBackground = 13.0;
constexpr CGFloat kMostVisitedTileImageContainerSquareCornerRadius = 16.0;
constexpr NSUInteger kMostVisitedMaximumVisibleItemsOnScreen = 4;
constexpr NSUInteger kMostVisitedMaximumVisibleItemsOnScreenWithAIMModule =
    kMostVisitedMaximumVisibleItemsOnScreen - 1;

constexpr CGFloat kContainerTopInset = 20.0;
constexpr CGFloat kContainerHorizontalInset = 20.0;
constexpr CGFloat kMostVisitedClearIconContainerSize = 24.0;

}  // namespace

// In `kAimAsModule`, Most Visited Tiles icons are shrunk and rendered without
// backgrounds, which reduces the row height. Returning this net reduction
// allows header space redistribution to compensate and maintain consistent feed
// peeking.
CGFloat MostVisitedHeightReductionForAimModule() {
  if (!IsAimEnabledInNtp() ||
      ntp_tiles::GetAimButtonRefactorArm() !=
          ntp_tiles::AimButtonRefactorArm::kAimAsModule) {
    return 0.0;
  }
  const CGFloat baseline_title_spacing =
      IsNewTabPageUICleanupEnabled() ? kMostVisitedIconTitleSpacingUICleanup
                                     : kMostVisitedIconTitleSpacing;
  const CGFloat icon_container_reduction =
      kMagicStackImageContainerWidth - kMostVisitedClearIconContainerSize;
  const CGFloat title_spacing_increase =
      kMostVisitedIconTitleSpacingWithoutBackground - baseline_title_spacing;
  const CGFloat container_top_inset_increase =
      kMostVisitedContainerInsets.top - kMagicStackContainerInsets.top;
  return icon_container_reduction - title_spacing_increase -
         container_top_inset_increase;
}

CGFloat MostVisitedIconTitleSpacing() {
  if (ntp_tiles::GetAimButtonRefactorArm() ==
      ntp_tiles::AimButtonRefactorArm::kAimAsModule) {
    return kMostVisitedIconTitleSpacingWithoutBackground;
  }
  if (IsNewTabPageUICleanupEnabled()) {
    return kMostVisitedIconTitleSpacingUICleanup;
  }
  return kMostVisitedIconTitleSpacing;
}

CGFloat MostVisitedIconContainerSize() {
  if (ntp_tiles::GetAimButtonRefactorArm() ==
      ntp_tiles::AimButtonRefactorArm::kAimAsModule) {
    return kMostVisitedClearIconContainerSize;
  }
  return kMagicStackImageContainerWidth;
}

CGFloat MostVisitedIconContainerCornerRadius() {
  return IsNewTabPageUICleanupEnabled()
             ? kMostVisitedTileImageContainerSquareCornerRadius
             : (kMagicStackImageContainerWidth / 2);
}

NSUInteger MostVisitedMaximumVisibleItemsOnScreen(BOOL aim_available) {
  if (ntp_tiles::GetAimButtonRefactorArm() ==
      ntp_tiles::AimButtonRefactorArm::kAimAsModule) {
    return aim_available ? kMostVisitedMaximumVisibleItemsOnScreenWithAIMModule
                         : kMostVisitedMaximumVisibleItemsOnScreen;
  }
  return kMostVisitedMaximumVisibleItemsOnScreen;
}

const NSDirectionalEdgeInsets kMostVisitedContainerInsets = {
    kContainerTopInset, kContainerHorizontalInset, 0.0,
    kContainerHorizontalInset};

const CGFloat kMostVisitedTileIconSize = 56.0;
