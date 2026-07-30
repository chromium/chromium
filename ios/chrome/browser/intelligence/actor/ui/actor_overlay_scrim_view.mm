// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_scrim_view.h"

namespace {
// Accessibility identifier for the overlay scrim view.
NSString* const kActorOverlayScrimAccessibilityIdentifier =
    @"ActorOverlayScrimAccessibilityIdentifier";

// Alpha value used for the actor overlay scrim color.
constexpr CGFloat kActorOverlayScrimViewAlpha = 0.15f;
}  // namespace

@implementation ActorOverlayScrimView

- (instancetype)initWithScrimColor:(UIColor*)scrimColor {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.accessibilityIdentifier = kActorOverlayScrimAccessibilityIdentifier;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.backgroundColor =
        [scrimColor colorWithAlphaComponent:kActorOverlayScrimViewAlpha];
  }
  return self;
}

@end
