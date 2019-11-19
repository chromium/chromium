// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"

#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PrimaryActionButton

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self)
    [self updateStyling];
  return self;
}

- (id)initWithCoder:(NSCoder*)aDecoder {
  self = [super initWithCoder:aDecoder];
  if (self)
    [self updateStyling];
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self updateStyling];
}

- (void)updateStyling {
  self.hasOpaqueBackground = YES;

  UIColor* hintColor = UIColor.cr_systemBackgroundColor;
  UIColor* inkColor = [UIColor colorWithWhite:1 alpha:0.2f];
  UIColor* backgroundColor = [UIColor colorNamed:kBlueColor];
  UIColor* disabledColor = [UIColor colorNamed:kDisabledTintColor];
  UIColor* titleColor = [UIColor colorNamed:kSolidButtonTextColor];

  if (@available(iOS 13, *)) {
    // As of iOS 13 Beta 3, MDCFlatButton has a bug updating it's colors
    // automatically. Here the colors are resolved and passed instead.
    // TODO(crbug.com/983224): Clean up this once the bug is fixed.
    hintColor =
        [hintColor resolvedColorWithTraitCollection:self.traitCollection];
    inkColor = [inkColor resolvedColorWithTraitCollection:self.traitCollection];
    backgroundColor =
        [backgroundColor resolvedColorWithTraitCollection:self.traitCollection];
    disabledColor =
        [disabledColor resolvedColorWithTraitCollection:self.traitCollection];
    titleColor =
        [titleColor resolvedColorWithTraitCollection:self.traitCollection];
  }

  self.underlyingColorHint = hintColor;
  self.inkColor = inkColor;
  [self setBackgroundColor:backgroundColor forState:UIControlStateNormal];
  [self setBackgroundColor:disabledColor forState:UIControlStateDisabled];
  [self setTitleColor:titleColor forState:UIControlStateNormal];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 13, *)) {
    if ([self.traitCollection
            hasDifferentColorAppearanceComparedToTraitCollection:
                previousTraitCollection]) {
      // As of iOS 13 Beta 3, MDCFlatButton doesn't update it's colors
      // automatically. This line forces it.
      [self updateStyling];
    }
  }
}
@end
