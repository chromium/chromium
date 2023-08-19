// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/share_extension/ui_util.h"

namespace ui_util {

const CGFloat kAnimationDuration = 0.3;

void ConstrainAllSidesOfViewToView(UIView* container, UIView* filler) {
  [NSLayoutConstraint activateConstraints:@[
    [filler.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [filler.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
    [filler.topAnchor constraintEqualToAnchor:container.topAnchor],
    [filler.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
  ]];
}

}  // namespace ui_util
