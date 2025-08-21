// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GLOW_EFFECT_GLOW_EFFECT_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GLOW_EFFECT_GLOW_EFFECT_API_H_

#import <UIKit/UIKit.h>

// Public API for the Glow Effect.
@protocol GlowEffect <NSObject>

// The duration in seconds for one full 360-degree rotation.
@property(nonatomic, assign) CFTimeInterval rotationSpeedDuration;

// The duration of the glow fade-in and fade-out animation.
@property(nonatomic, assign) CFTimeInterval fadeAnimationDuration;

// Starts the glow animation.
- (void)startGlow;

// Stops the glow animation.
- (void)stopGlow;

@end

namespace ios {
namespace provider {

// Creates and returns a view that applies a glow effect on its border. `frame`
// is optional.
// TODO(crbug.com/440074963): This is used in a prototype, it has not been UX
// approved yet and should not be reused for now.
UIView<GlowEffect>* CreateGlowEffect(CGRect frame,
                                     CGFloat cornerRadius,
                                     CGFloat borderWidth);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_GLOW_EFFECT_GLOW_EFFECT_API_H_
