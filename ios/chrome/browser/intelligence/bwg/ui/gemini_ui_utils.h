// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_UI_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_UI_UTILS_H_

#import <UIKit/UIKit.h>

// Common UI elements for Gemini.
@interface GeminiUIUtils : NSObject

// Returns the branded version of the Gemini symbol with a `pointSize`.
+ (UIImage*)brandedGeminiSymbolWithPointSize:(CGFloat)pointSize;

// Create the Gemini logo with a diagonal linear gradient color palette.
+ (UIImage*)createGradientGeminiLogo:(CGFloat)pointSize;

// Returns an attributed string for `text` where `targetWord` is rendered with a
// linear gradient color palette defined by `colors` and matching `font`.
// Note: Callers displaying this attributed string in a UILabel should set the
// label's `accessibilityLabel` to the unformatted plain text to ensure
// VoiceOver reads the string seamlessly without NSTextAttachment artifacts.
+ (NSAttributedString*)attributedStringByReplacingWord:(NSString*)targetWord
                                                inText:(NSString*)text
                                                  font:(UIFont*)font
                                                colors:
                                                    (NSArray<UIColor*>*)colors;

// Returns an attributed string for `title` where the word "Gemini" is
// replaced with the branded gradient Gemini logo matching `font`.
+ (NSAttributedString*)attributedStringWithGradientGeminiForTitle:
                           (NSString*)title
                                                             font:(UIFont*)font;

// Returns the expected content height of `targetView` when constrained to
// `containerWidth`. Measures using `containerWidth` when established (> 0) so
// that text wrapping heights are computed correctly, preventing clipping. Falls
// back to unconstrained measurement when `containerWidth` is 0 (i.e. at early
// initialization/viewDidLoad, before layout is resolved).
+ (CGFloat)contentHeightForView:(UIView*)targetView
             withContainerWidth:(CGFloat)containerWidth;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_UI_UTILS_H_
