// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Returns the height of the app bar in portrait.
CGFloat AppBarHeightPortrait();

// The height of the app bar when in fullscreen (portrait).
extern const CGFloat kAppBarHeightFullscreen;

// Returns the height of the app bar in landscape.
CGFloat AppBarHeightLandscape();

// Accessibility identifier for the assistant button.
extern NSString* const kAppBarAssistantButtonId;

// Accessibility identifier for the app bar tab grid button.
extern NSString* const kAppBarTabGridButtonIdentifier;

// Accessibility identifier for the app bar new tab button.
extern NSString* const kAppBarNewTabButtonIdentifier;

// The histogram name for tracking when the assistant button is tapped.
extern const char kAppBarAssistantButtonTappedHistogram[];

// The histogram name for tracking the settled state of the assistant button
// after startup.
extern const char kAppBarAssistantButtonStateOnLoadHistogram[];

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSTANTS_H_
