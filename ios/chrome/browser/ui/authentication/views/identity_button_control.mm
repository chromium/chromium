// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"

#import <MaterialComponents/MaterialRipple.h>

#import "base/check.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/views/identity_view.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kIdentityButtonControlRadius = 8.;
// Sizes.
const CGFloat kArrowDownSize = 24.;
// Distances/margins.
const CGFloat kArrowDownMargin = 12.;

}  // namespace

@interface IdentityButtonControl ()

@property(nonatomic, strong) IdentityView* identityView;
// Ripple effect when the user starts or stop a touch in the view.
@property(nonatomic, strong) MDCRippleView* rippleView;
// Image View for the arrow (down or left according to |style|, see the
// |arrowDirection| property), letting the user know that more profiles can be
// selected.
@property(nonatomic, strong) UIImageView* arrowImageView;

@end

@implementation IdentityButtonControl

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.accessibilityIdentifier = kIdentityButtonControlIdentifier;
    self.layer.cornerRadius = kIdentityButtonControlRadius;
    self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    // Adding view elements inside.
    // Ink view.
    _rippleView = [[MDCRippleView alloc] initWithFrame:CGRectZero];
    _rippleView.layer.cornerRadius = kIdentityButtonControlRadius;
    _rippleView.rippleStyle = MDCRippleStyleBounded;
    _rippleView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_rippleView];

    // Down or right arrow.
    _arrowImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _arrowImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _arrowDirection = IdentityButtonControlArrowDown;
    [self updateArrowDirection];
    _arrowImageView.tintColor = UIColor.cr_labelColor;
    [self addSubview:_arrowImageView];

    // Main view with avatar, name and email.
    _identityView = [[IdentityView alloc] initWithFrame:CGRectZero];
    _identityView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_identityView];

    // Layout constraints.
    NSDictionary* views = @{
      @"identityview" : _identityView,
      @"arrow" : _arrowImageView,
    };
    NSDictionary* metrics = @{
      @"ArrowMargin" : @(kArrowDownMargin),
      @"ArrowSize" : @(kArrowDownSize),
    };
    NSArray* constraints = @[
      // Horizontal constraints.
      @"H:|[identityview]-(ArrowMargin)-[arrow]-(ArrowMargin)-|",
      // Vertical constraints.
      @"V:|[identityview]|",
      // Size constraints.
      @"H:[arrow(ArrowSize)]",
      @"V:[arrow(ArrowSize)]",
    ];
    AddSameCenterYConstraint(self, _arrowImageView);
    ApplyVisualConstraintsWithMetrics(constraints, views, metrics);

    if (@available(iOS 13.4, *)) {
      [self addInteraction:[[ViewPointerInteraction alloc] init]];
    }

    // Accessibility.
    self.isAccessibilityElement = YES;
    self.accessibilityTraits = UIAccessibilityTraitButton;
  }
  return self;
}

#pragma mark - Setter

- (void)setIdentityAvatar:(UIImage*)identityAvatar {
  [self.identityView setAvatar:identityAvatar];
}

- (void)setIdentityName:(NSString*)name email:(NSString*)email {
  DCHECK(email);
  if (!name.length) {
    [self.identityView setTitle:email subtitle:nil];
    self.accessibilityLabel = email;
  } else {
    [self.identityView setTitle:name subtitle:email];
    self.accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", name, email];
  }
}

#pragma mark - Properties

- (void)setArrowDirection:(IdentityButtonControlArrowDirection)arrowDirection {
  _arrowDirection = arrowDirection;
  [self updateArrowDirection];
}

- (void)setMinimumTopMargin:(CGFloat)minimumTopMargin {
  self.identityView.minimumTopMargin = minimumTopMargin;
}

- (CGFloat)minimumTopMargin {
  return self.identityView.minimumTopMargin;
}

- (void)setMinimumBottomMargin:(CGFloat)minimumBottomMargin {
  self.identityView.minimumBottomMargin = minimumBottomMargin;
}

- (CGFloat)minimumBottomMargin {
  return self.identityView.minimumBottomMargin;
}

- (void)setAvatarSize:(CGFloat)avatarSize {
  self.identityView.avatarSize = avatarSize;
}

- (CGFloat)avatarSize {
  return self.identityView.avatarSize;
}

- (void)setTitleSubtitleMargin:(CGFloat)titleSubtitleMargin {
  self.identityView.titleSubtitleMargin = titleSubtitleMargin;
}

- (CGFloat)titleSubtitleMargin {
  return self.identityView.titleSubtitleMargin;
}

- (void)setTitleFont:(UIFont*)titleFont {
  self.identityView.titleFont = titleFont;
}

- (UIFont*)titleFont {
  return self.identityView.titleFont;
}

- (void)setSubtitleFont:(UIFont*)subtitleFont {
  self.identityView.subtitleFont = subtitleFont;
}

- (UIFont*)subtitleFont {
  return self.identityView.subtitleFont;
}

#pragma mark - Private

- (CGPoint)locationFromTouches:(NSSet*)touches {
  UITouch* touch = [touches anyObject];
  return [touch locationInView:self];
}

- (void)updateArrowDirection {
  UIImage* image = nil;
  UIColor* tintColor = nil;
  switch (self.arrowDirection) {
    case IdentityButtonControlArrowRight:
      image = [UIImage imageNamed:@"identity_picker_view_arrow_right"];
      tintColor = [UIColor colorNamed:kTextTertiaryColor];
      break;
    case IdentityButtonControlArrowDown:
      image = [UIImage imageNamed:@"identity_picker_view_arrow_down"];
      break;
  }
  DCHECK(image);
  self.arrowImageView.image =
      [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  self.arrowImageView.tintColor = tintColor;
}

#pragma mark - UIResponder

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesBegan:touches withEvent:event];
  CGPoint location = [self locationFromTouches:touches];
  [self.rippleView beginRippleTouchDownAtPoint:location
                                      animated:YES
                                    completion:nil];
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesEnded:touches withEvent:event];
  [self.rippleView beginRippleTouchUpAnimated:YES completion:nil];
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesCancelled:touches withEvent:event];
  [self.rippleView beginRippleTouchUpAnimated:YES completion:nil];
}

@end
