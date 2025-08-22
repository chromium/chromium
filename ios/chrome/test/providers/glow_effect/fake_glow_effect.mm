// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/glow_effect/fake_glow_effect.h"

#import "base/check.h"
#import "ios/public/provider/chrome/browser/glow_effect/glow_effect_api.h"

@implementation FakeGlowEffectView

@synthesize rotationSpeedDuration = _rotationSpeedDuration;
@synthesize fadeAnimationDuration = _fadeAnimationDuration;

- (void)startGlow {
  self.glowAnimationInProgress = YES;
}

- (void)stopGlow {
  self.glowAnimationInProgress = NO;
}

@end

namespace ios {
namespace provider {

UIView<GlowEffect>* CreateGlowEffect(CGRect frame,
                                     CGFloat cornerRadius,
                                     CGFloat borderWidth) {
  return [[FakeGlowEffectView alloc] initWithFrame:frame];
}

}  // namespace provider
}  // namespace ios
