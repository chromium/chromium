// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"

#import <MaterialComponents/MaterialButtons.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrimaryActionButton : MDCFlatButton
@end

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
  // Disable the default MDC behavior of supporting separate fonts per
  // button state.
  self.enableTitleFontForState = NO;
  self.hasOpaqueBackground = YES;
  self.pointerInteractionEnabled = YES;
  self.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

  UIColor* hintColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  UIColor* inkColor = [UIColor colorWithWhite:1 alpha:0.2f];
  UIColor* backgroundColor = [UIColor colorNamed:kBlueColor];
  UIColor* disabledColor = [UIColor colorNamed:kDisabledTintColor];
  UIColor* titleColor = [UIColor colorNamed:kSolidButtonTextColor];

  // As of iOS 13 Beta 3, MDCFlatButton has a bug updating it's colors
  // automatically. Here the colors are resolved and passed instead.
  // TODO(crbug.com/983224): Clean up this once the bug is fixed.
  hintColor = [hintColor resolvedColorWithTraitCollection:self.traitCollection];
  inkColor = [inkColor resolvedColorWithTraitCollection:self.traitCollection];
  backgroundColor =
      [backgroundColor resolvedColorWithTraitCollection:self.traitCollection];
  disabledColor =
      [disabledColor resolvedColorWithTraitCollection:self.traitCollection];
  titleColor =
      [titleColor resolvedColorWithTraitCollection:self.traitCollection];

  self.underlyingColorHint = hintColor;
  self.inkColor = inkColor;
  [self setBackgroundColor:backgroundColor forState:UIControlStateNormal];
  [self setBackgroundColor:disabledColor forState:UIControlStateDisabled];
  [self setTitleColor:titleColor forState:UIControlStateNormal];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    // As of iOS 13 Beta 3, MDCFlatButton doesn't update it's colors
    // automatically. This line forces it.
    [self updateStyling];
  }
}
@end

UIButton* CreatePrimaryActionButton() {
  return [[PrimaryActionButton alloc] init];
}
