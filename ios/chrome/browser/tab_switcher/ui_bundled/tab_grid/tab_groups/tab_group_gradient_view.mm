// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_gradient_view.h"

#import "base/check.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The transparency for the background gradient.
const CGFloat kBackgroundGradientAlpha = 0.6;

}  // namespace

@implementation TabGroupGradientView

- (instancetype)initWithColors:(NSArray*)colors {
  CHECK(colors.count == 3);
  self = [super initWithColors:colors
                     locations:@[ @0.0, @0.15, @1.0 ]
                    startPoint:CGPointMake(0.5, 0)
                      endPoint:CGPointMake(0.5, 1)];
  if (self) {
    UIView* background = [[UIView alloc] init];
    background.translatesAutoresizingMaskIntoConstraints = NO;
    background.backgroundColor =
        [UIColor.blackColor colorWithAlphaComponent:kBackgroundGradientAlpha];

    [self addSubview:background];
    AddSameConstraints(self, background);
  }
  return self;
}

@end
