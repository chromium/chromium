// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbar_background.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation TabGridToolbarBackground {
  UIView* _scrolledOverContentBackgroundView;
  UIView* _scrolledToEdgeBackgroundView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Initialize subviews.
    // Background when the content is scrolled to the middle.
    _scrolledOverContentBackgroundView = CreateTabGridOverContentBackground();
    _scrolledOverContentBackgroundView.hidden = YES;

    // Background when the content is scrolled to the edge of the screen.
    _scrolledToEdgeBackgroundView = CreateTabGridScrolledToEdgeBackground();

    _scrolledOverContentBackgroundView
        .translatesAutoresizingMaskIntoConstraints = NO;

    _scrolledToEdgeBackgroundView.translatesAutoresizingMaskIntoConstraints =
        NO;

    // Add subviews.
    [self addSubview:_scrolledOverContentBackgroundView];
    [self addSubview:_scrolledToEdgeBackgroundView];

    AddSameConstraints(_scrolledOverContentBackgroundView, self);
    AddSameConstraints(_scrolledToEdgeBackgroundView, self);
  }
  return self;
}

- (void)setScrolledOverContentBackgroundViewHidden:(BOOL)hidden {
  _scrolledOverContentBackgroundView.hidden = hidden;
}

- (void)setScrolledToEdgeBackgroundViewHidden:(BOOL)hidden {
  _scrolledToEdgeBackgroundView.hidden = hidden;
}

@end
