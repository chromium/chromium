// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>

namespace intelligence::actor {

// Standard spacing constants for worklog UI elements.
inline constexpr CGFloat kSpacingTiny = 4.0;
inline constexpr CGFloat kSpacingSmall = 8.0;
inline constexpr CGFloat kSpacingMedium = 12.0;
inline constexpr CGFloat kSpacingLarge = 16.0;

// Layout dimension for the timeline gutter/connector lines.
inline constexpr CGFloat kTimelineGutterWidth = 50.0;

}  // namespace intelligence::actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_CONSTANTS_H_
