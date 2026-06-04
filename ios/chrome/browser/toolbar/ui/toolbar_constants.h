// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Accessibility identifiers for the toolbar views.
extern NSString* const kPrimaryToolbarViewIdentifier;
extern NSString* const kSecondaryToolbarViewIdentifier;

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

// The height of the promo banner displayed in the toolbar.
extern const CGFloat kToolbarPromoBannerHeight;

// The height of the toolbar.
extern const CGFloat kToolbarHeight;
// The height of the toolbar when at the top on iPhone portrait.
extern const CGFloat kTopToolbarIPhonePortraitHeight;
// The height of the location bar in the toolbar.
extern const CGFloat kLocationBarHeight;
// The height of the location bar in the toolbar when at the top on iPhone
// portrait.
extern const CGFloat kTopLocationBarIPhonePortraitHeight;
// The padding in the toolbar.
extern const CGFloat kToolbarPadding;
// The padding in the toolbar when at the top on iPhone portrait..
extern const CGFloat kTopToolbarIPhonePortraitPadding;

// The height of the toolbar when in fullscreen on iPad.
extern const CGFloat kTopToolbarIPadHeightFullscreen;
// The height of the toolbar when in fullscreen.
extern const CGFloat kToolbarHeightFullscreen;
// The height of the toolbar when at the top in fullscreen on iPhone portrait.
extern const CGFloat kTopToolbarIPhonePortraitHeightFullscreen;
// The height of the location bar when in fullscreen.
extern const CGFloat kLocationBarHeightFullscreen;
// The padding in the toolbar when in fullscreen.
extern const CGFloat kToolbarPaddingFullscreen;
// Vertical offset of the outer separator.
extern const CGFloat kOuterSeparatorVerticalOffset;
// Additional height added to the bottom omnibox when attached above the
// keyboard.
extern const CGFloat kKeyboardAttachedOmniboxBottomPadding;
// Additional height added to the bottom omnibox when attached above the
// keyboard in landscape orientation.
extern const CGFloat kKeyboardAttachedOmniboxBottomPaddingLandscape;

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_CONSTANTS_H_
