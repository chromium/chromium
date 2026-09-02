// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// Accessibility identifier for the actuation header view.
inline constexpr NSString* const kActuationHeaderAccessibilityIdentifier =
    @"ActuationHeaderAccessibilityIdentifier";

// Accessibility identifier for the compact worklog view.
inline constexpr NSString* const kCompactWorklogAccessibilityIdentifier =
    @"CompactWorklogAccessibilityIdentifier";

// Accessibility identifier for the full worklog scroll view.
inline constexpr NSString* const kFullWorklogScrollViewAccessibilityIdentifier =
    @"FullWorklogScrollViewAccessibilityIdentifier";

// TODO(crbug.com/553535673): Remove C++ namespace for UI constants.
namespace intelligence::actor {

// Standard spacing constants for worklog UI elements.
inline constexpr CGFloat kSpacingTiny = 4.0;
inline constexpr CGFloat kSpacingSmall = 8.0;
inline constexpr CGFloat kSpacingMedium = 12.0;
inline constexpr CGFloat kSpacingLarge = 16.0;

// Layout dimension for the timeline gutter/connector lines.
inline constexpr CGFloat kTimelineGutterWidth = 50.0;

// The fixed layout height of the actor tool chip view.
inline constexpr CGFloat kToolChipHeight = 32.0;

}  // namespace intelligence::actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_CONSTANTS_H_
