// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The width of the app bar when in landscape.
extern const CGFloat kAppBarHeight;

// The height of the app bar when in fullscreen (portrait).
extern const CGFloat kAppBarHeightFullscreen;

// The height of the app bar when in landscape.
extern const CGFloat kAppBarHeightLandscape;

// The corner radius for the app bar and app content view.
extern const CGFloat kAppBarCornerRadius;

// Accessibility identifier for the assistant button.
extern NSString* const kAppBarAssistantButtonId;

// Accessibility identifier for the app bar tab grid button.
extern NSString* const kAppBarTabGridButtonIdentifier;

// Accessibility identifier for the app bar new tab button.
extern NSString* const kAppBarNewTabButtonIdentifier;

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSTANTS_H_
