// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_most_visited_tile_view.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_constants.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_layout_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsMostVisitedCell ()

@property(nonatomic, strong) NTPMostVisitedTileView* mostVisitedTile;

@end

@implementation ContentSuggestionsMostVisitedCell : MDCCollectionViewCell

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _mostVisitedTile = [[NTPMostVisitedTileView alloc] initWithFrame:frame];
    [self.contentView addSubview:_mostVisitedTile];
    _mostVisitedTile.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self.contentView, _mostVisitedTile);
    self.isAccessibilityElement = YES;
  }
  return self;
}

- (FaviconView*)faviconView {
  return self.mostVisitedTile.faviconView;
}

- (UILabel*)titleLabel {
  return self.mostVisitedTile.titleLabel;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];

  [UIView transitionWithView:self
                    duration:ios::material::kDuration8
                     options:UIViewAnimationOptionCurveEaseInOut
                  animations:^{
                    self.alpha = highlighted ? 0.5 : 1.0;
                  }
                  completion:nil];
}

+ (CGSize)defaultSize {
  return MostVisitedCellSize(
      UIApplication.sharedApplication.preferredContentSizeCategory);
}

- (CGSize)intrinsicContentSize {
  return [[self class] defaultSize];
}

@end
