// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/test_share_kit_avatar_primitive.h"

namespace {

// The size in points of the avatar primitve view.
const CGFloat avatarPrimitiveSize = 30;

}  // namespace

@implementation TestShareKitAvatarPrimitive

- (UIView*)view {
  UIView* view = [[UIView alloc]
      initWithFrame:CGRectMake(0, 0, avatarPrimitiveSize, avatarPrimitiveSize)];
  view.backgroundColor = UIColor.redColor;
  return view;
}

- (void)resolve {
}

@end
