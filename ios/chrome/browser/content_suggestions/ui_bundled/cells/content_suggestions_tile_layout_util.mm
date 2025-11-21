// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_tile_layout_util.h"

namespace {

// Horizontal spacing between columns of tiles.
const int kContentSuggestionsTilesHorizontalSpacingRegular = 19;
const int kContentSuggestionsTilesHorizontalSpacingCompact = 5;

}  // namespace

CGFloat ContentSuggestionsTilesHorizontalSpacing(
    UITraitCollection* trait_collection) {
  return (trait_collection.horizontalSizeClass !=
              UIUserInterfaceSizeClassCompact &&
          trait_collection.verticalSizeClass != UIUserInterfaceSizeClassCompact)
             ? kContentSuggestionsTilesHorizontalSpacingRegular
             : kContentSuggestionsTilesHorizontalSpacingCompact;
}
