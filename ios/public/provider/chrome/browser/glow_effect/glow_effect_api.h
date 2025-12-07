// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GLOW_EFFECT_GLOW_EFFECT_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GLOW_EFFECT_GLOW_EFFECT_API_H_

#import <UIKit/UIKit.h>

// Represents the current state of the glow effect animation.
enum class GlowState {
  // The glow effect is not visible and no animations are active.
  kStopped,
  // The glow and stroke are visible and rotating continuously.
  kRunning,
  // The rotation is decelerating to a stop. The stroke is still visible but
  // will fade out upon completion.
  kStoppingRotation,
};

/// A protocol for a view that displays a glow effect.
@protocol GlowEffect

/// The duration of one full rotation of the glow effect.
@property(nonatomic, assign) CFTimeInterval rotationSpeedDuration;

/// The duration of the fade-in and fade-out animations.
@property(nonatomic, assign) CFTimeInterval fadeAnimationDuration;

/// The current state of the glow effect animation.
@property(nonatomic, assign, readonly) GlowState glowState;

/// Starts the glow effect, making it visible and beginning the rotation.
- (void)startGlow;

/// Stops the glow effect, fading it out and stopping any animations.
- (void)stopGlow;

/// Stops the rotation of the glow and stroke, hides the stroke, but leaves the
/// glow effect visible at a fixed angle.
- (void)stopRotation;

@end

namespace ios {
namespace provider {

// Creates and returns a view that applies a glow effect on its border. To apply
// the effect on a view, constraint it with a negative inset of `borderWidth`.
// `frame` is optional.
// TODO(crbug.com/440074963): This is used in a prototype, it has not been UX
// approved yet and should not be reused for now.
UIView<GlowEffect>* CreateGlowEffect(CGRect frame,
                                     CGFloat cornerRadius,
                                     CGFloat borderWidth);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GLOW_EFFECT_GLOW_EFFECT_API_H_
