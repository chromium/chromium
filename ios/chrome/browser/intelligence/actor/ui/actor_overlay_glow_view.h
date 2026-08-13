// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_GLOW_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_GLOW_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_glow_view_data.h"

// A view that renders a glowing border around the overlay bounds.
@interface ActorOverlayGlowView : UIView

// The corner radii for each corner of the glow view.
@property(nonatomic, assign) CornerRadii cornerRadii;

// Initializes the view with the color of the glowing border.
- (instancetype)initWithGlowColor:(UIColor*)glowColor NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_GLOW_VIEW_H_
