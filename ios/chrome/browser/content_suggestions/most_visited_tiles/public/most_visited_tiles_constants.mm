// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/public/most_visited_tiles_constants.h"

#import "components/ntp_tiles/features.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"

namespace {

constexpr CGFloat kMostVisitedIconTitleSpacing = 10.0;
constexpr CGFloat kMostVisitedIconTitleSpacingUICleanup = 8.0;
constexpr CGFloat kMostVisitedIconTitleSpacingWithoutBackground = 13.0;

constexpr CGFloat kContainerTopInset = 20.0;
constexpr CGFloat kContainerHorizontalInset = 20.0;
constexpr CGFloat kMostVisitedClearIconContainerSize = 24.0;

}  // namespace

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

const NSDirectionalEdgeInsets kMostVisitedContainerInsets = {
    kContainerTopInset, kContainerHorizontalInset, 0.0,
    kContainerHorizontalInset};

const CGFloat kMostVisitedTileImageContainerSquareCornerRadius = 16.0;

const CGFloat kMostVisitedTileIconSize = 56.0;
