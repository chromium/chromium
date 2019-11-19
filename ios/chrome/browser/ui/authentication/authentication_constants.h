// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Header image sizes.
extern const CGFloat kAuthenticationHeaderImageHeight;

// Font sizes
extern const UIFontTextStyle kAuthenticationTitleFontStyle;
extern const UIFontTextStyle kAuthenticationTextFontStyle;

// Horizontal margin between the container view and any elements inside.
extern const CGFloat kAuthenticationHorizontalMargin;
// Vertical margin between the header image and the main title.
extern const CGFloat kAuthenticationHeaderTitleMargin;

// Alpha for the separator color.
extern const CGFloat kAuthenticationSeparatorColorAlpha;
// Height of the separator.
extern const CGFloat kAuthenticationSeparatorHeight;

// Header image name.
extern NSString* const kAuthenticationHeaderImageName;

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_CONSTANTS_H_
