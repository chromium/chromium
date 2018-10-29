// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/most_visited_shortcut_cell.h"

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_most_visited_tile_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation MostVisitedShortcutCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _tile = [[NTPMostVisitedTileView alloc] init];
    _tile.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_tile];
    AddSameConstraints(self.contentView, _tile);
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.tile.titleLabel.text = nil;
}

@end
