// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CROSSFADE_LABEL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CROSSFADE_LABEL_H_

#import <UIKit/UIKit.h>

// Label that provides the ability to crossfade with an overlaid (nearly)
// identical label, to allow animating attributes that are not normally
// animatable (for example: textColor or even text attributes like
// strikethrough).
//
// Example usage:
//   [crossfadeLabel setUpCrossfadeWithTextColor:greenColor
//                                attributedText:attributedTextForCrossfade];
//   [UIView animateWithDuration:1.0
//       animations:^{
//         [crossfadeLabel crossfade];
//       }
//       completion:^(BOOL finished) {
//         [crossfadeLabel cleanupAfterCrossfade];
//       }];
//
@interface CrossfadeLabel : UILabel

// Creates a copy of this label and applies the given `textColor` and
// `attributedText` to the copy, and sets the alpha of the copy to 0. The copy
// is added to the superview to prepare for the crossfade.
- (void)setUpCrossfadeWithTextColor:(UIColor*)textColor
                     attributedText:(NSAttributedString*)attributedText;

// Changes the alpha of this label and its copy, in order to perform the
// crossfade. Should be called in the `animations` block of a UIView animation.
- (void)crossfade;

// Cleans up the copy used to perform the crossfade. Should be called in the
// completion block of a UIView animation.
- (void)cleanupAfterCrossfade;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CROSSFADE_LABEL_H_
