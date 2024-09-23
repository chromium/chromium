// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Header image sizes.
extern const CGFloat kAuthenticationHeaderImageHeight;

// Height/Width of the avatar in authentication context.
extern const CGFloat kAccountProfilePhotoDimension;

// Horizontal margin between the container view and any elements inside.
extern const CGFloat kAuthenticationHorizontalMargin;
// Vertical margin between the header image and the main title.
extern const CGFloat kAuthenticationHeaderTitleMargin;

// Alpha for the separator color.
extern const CGFloat kAuthenticationSeparatorColorAlpha;
// Height of the separator.
extern const CGFloat kAuthenticationSeparatorHeight;

// Accessibility identifier for the Signin/Sync screen.
extern NSString* const kSigninSyncScreenAccessibilityIdentifier;

// Accessibility identifier for the 'Undo' button in signin snackbar.
extern NSString* const kSigninSnackbarUndo;

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_CONSTANTS_H_
