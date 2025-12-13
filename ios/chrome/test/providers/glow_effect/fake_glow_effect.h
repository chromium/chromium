// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_GLOW_EFFECT_FAKE_GLOW_EFFECT_H_
#define IOS_CHROME_TEST_PROVIDERS_GLOW_EFFECT_FAKE_GLOW_EFFECT_H_

#import "ios/public/provider/chrome/browser/glow_effect/glow_effect_api.h"

// A fake implementation of the GlowEffect protocol for tests.
@interface FakeGlowEffectView : UIView <GlowEffect>

@property(nonatomic, assign) BOOL glowAnimationInProgress;
@property(nonatomic, assign) BOOL glowRotationInProgress;

@end

#endif  // IOS_CHROME_TEST_PROVIDERS_GLOW_EFFECT_FAKE_GLOW_EFFECT_H_
