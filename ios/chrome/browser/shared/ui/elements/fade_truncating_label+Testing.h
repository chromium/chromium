// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_FADE_TRUNCATING_LABEL_TESTING_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_FADE_TRUNCATING_LABEL_TESTING_H_

#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label.h"

// Testing category exposing a private method of FadeTruncatingLabel for tests.
@interface FadeTruncatingLabel (Testing)

/// Draws `attributedString` in `requestedRect`.
/// `applyGradient`: Whether gradient should be applied when drawing the text.
/// `alignmentOffset`: offset added to draw the text on the left of
/// `requestedRect`. Note: with NSLineBreakByClipping the text is always clipped
/// to the right even when the text is aligned to the right, with the offset the
/// text starts to draw on the left of `requestedRect`, this allow the text to
/// end inside of `requestedRect` clipping it on the left.
- (void)drawAttributedString:(NSAttributedString*)attributedString
                      inRect:(CGRect)requestedRect
               applyGradient:(BOOL)applyGradient
             alignmentOffset:(CGFloat)alignmentOffset;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_FADE_TRUNCATING_LABEL_TESTING_H_
