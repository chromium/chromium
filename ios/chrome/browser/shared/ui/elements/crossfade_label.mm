// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/crossfade_label.h"

#import "base/check.h"

@implementation CrossfadeLabel {
  // The label that will be overlayed on top in order to perform the crossfade.
  UILabel* _overlay;
}

- (void)setUpCrossfadeWithTextColor:(UIColor*)textColor
                     attributedText:(NSAttributedString*)attributedText {
  CHECK(self.superview);
  _overlay = [[UILabel alloc] initWithFrame:self.frame];

  // Copy over various attributes, so that the overlay will render the same as
  // this label.
  _overlay.font = self.font;
  _overlay.numberOfLines = self.numberOfLines;
  _overlay.lineBreakMode = self.lineBreakMode;

  // Set the modified properties, if given.
  _overlay.attributedText =
      attributedText ? attributedText : self.attributedText;
  _overlay.textColor = textColor ? textColor : self.textColor;

  // Add the overlay to the superview in an initially hidden way.
  _overlay.alpha = 0;
  [self.superview addSubview:_overlay];
}

- (void)crossfade {
  self.alpha = 0;
  _overlay.alpha = 1;
}

- (void)cleanupAfterCrossfade {
  // Copy over a few attributes that may have changed.
  self.attributedText = _overlay.attributedText;
  self.textColor = _overlay.textColor;

  // Show this label again and remove the overlay.
  self.alpha = 1;
  [_overlay removeFromSuperview];
  _overlay = nil;
}

@end
