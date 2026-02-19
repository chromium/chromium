// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Accessibility identifier for the toolbar view.
extern NSString* const kToolbarViewIdentifier;

// Accessibility identifiers for toolbar buttons.
extern NSString* const kToolbarBackButtonIdentifier;
extern NSString* const kToolbarForwardButtonIdentifier;
extern NSString* const kToolbarReloadButtonIdentifier;
extern NSString* const kToolbarStopButtonIdentifier;
extern NSString* const kToolbarShareButtonIdentifier;
extern NSString* const kToolbarTabGridButtonIdentifier;
extern NSString* const kToolbarToolsMenuButtonIdentifier;
extern NSString* const kToolbarOmniboxButtonIdentifier;
extern NSString* const kToolbarAssistantButtonIdentifier;

// The height of the toolbar.
extern const CGFloat kToolbarHeight;
// The height of the location bar in the toolbar.
extern const CGFloat kLocationBarHeight;
// The padding in the toolbar.
extern const CGFloat kToolbarPadding;

// The height of the toolbar when in fullscreen.
extern const CGFloat kToolbarHeightFullscreen;
// The height of the location bar when in fullscreen.
extern const CGFloat kLocationBarHeightFullscreen;
// The padding in the toolbar when in fullscreen.
extern const CGFloat kToolbarPaddingFullscreen;

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSTANTS_H_
