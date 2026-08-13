// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_GLOW_VIEW_DATA_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_GLOW_VIEW_DATA_H_

#import <CoreGraphics/CoreGraphics.h>

// Represents the radii for the four corners of a rectangular view.
struct CornerRadii {
  CGFloat topLeft = 0.0;
  CGFloat topRight = 0.0;
  CGFloat bottomLeft = 0.0;
  CGFloat bottomRight = 0.0;

  constexpr CornerRadii() = default;
  constexpr explicit CornerRadii(CGFloat defaultRadius)
      : topLeft(defaultRadius),
        topRight(defaultRadius),
        bottomLeft(defaultRadius),
        bottomRight(defaultRadius) {}
  constexpr CornerRadii(CGFloat topLeft,
                        CGFloat topRight,
                        CGFloat bottomLeft,
                        CGFloat bottomRight)
      : topLeft(topLeft),
        topRight(topRight),
        bottomLeft(bottomLeft),
        bottomRight(bottomRight) {}

  friend bool operator==(const CornerRadii&, const CornerRadii&) = default;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_GLOW_VIEW_DATA_H_
