// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_UI_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_UI_UTILS_H_

#import <UIKit/UIKit.h>

// Common UI elements for BWG.
@interface BWGUIUtils : NSObject

// Creates the primary button with the given title.
+ (UIButton*)createPrimaryButtonWithTitle:(NSString*)title;

// Creates the secondary button with the given title.
+ (UIButton*)createSecondaryButtonWithTitle:(NSString*)title;

// Returns the branded version of the Gemini symbol with a `pointSize`.
+ (UIImage*)brandedGeminiSymbolWithPointSize:(CGFloat)pointSize;

// Create the Gemini logo with a diagonal linear gradient color palette.
+ (UIImage*)createGradientGeminiLogo:(CGFloat)pointSize;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_UI_UTILS_H_
