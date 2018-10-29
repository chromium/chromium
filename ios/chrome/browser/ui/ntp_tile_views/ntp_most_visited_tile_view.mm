// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_most_visited_tile_view.h"

#import "ios/chrome/common/favicon/favicon_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NTPMostVisitedTileView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _faviconView = [[FaviconView alloc] init];
    _faviconView.font = [UIFont systemFontOfSize:22];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_faviconView.heightAnchor constraintEqualToConstant:32],
      [_faviconView.widthAnchor
          constraintEqualToAnchor:_faviconView.heightAnchor],
    ]];

    [self.imageContainerView addSubview:_faviconView];
    AddSameConstraints(self.imageContainerView, _faviconView);
  }
  return self;
}

@end
