// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_SCRIM_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_SCRIM_VIEW_H_

#import <UIKit/UIKit.h>

// A scrim view used for actor UI overlays.
@interface ActorOverlayScrimView : UIView

// Initializes the overlay scrim view with a scrim color.
- (instancetype)initWithScrimColor:(UIColor*)scrimColor
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_SCRIM_VIEW_H_
