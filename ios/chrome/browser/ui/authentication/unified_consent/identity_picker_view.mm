// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_picker_view.h"

#import <MaterialComponents/MaterialRipple.h>

#include "base/check.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_view.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kIdentityPickerViewRadius = 8.;
// Sizes.
const CGFloat kArrowDownSize = 24.;
// Distances/margins.
const CGFloat kArrowDownMargin = 12.;

}  // namespace

@interface IdentityPickerView ()

@property(nonatomic, strong) IdentityView* identityView;
// Ripple effect when the user starts or stop a touch in the view.
@property(nonatomic, strong) MDCRippleView* rippleView;
// Image View for the down arrow, letting the user know that more profiles can
// be selected.
@property(nonatomic, strong) UIImageView* arrowDownImageView;

@end

@implementation IdentityPickerView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.accessibilityIdentifier = kIdentityPickerViewIdentifier;
    self.layer.cornerRadius = kIdentityPickerViewRadius;
    self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    // Adding view elements inside.
    // Ink view.
    _rippleView = [[MDCRippleView alloc] initWithFrame:CGRectZero];
    _rippleView.layer.cornerRadius = kIdentityPickerViewRadius;
    _rippleView.rippleStyle = MDCRippleStyleBounded;
    _rippleView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_rippleView];

    // Arrow down.
    _arrowDownImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _arrowDownImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _arrowDownImageView.image =
        [[UIImage imageNamed:@"identity_picker_view_arrow_down"]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _arrowDownImageView.tintColor = UIColor.cr_labelColor;
    [self addSubview:_arrowDownImageView];

    // Main view with avatar, name and email.
    _identityView = [[IdentityView alloc] initWithFrame:CGRectZero];
    _identityView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_identityView];

    // Layout constraints.
    NSDictionary* views = @{
      @"identityview" : _identityView,
      @"arrow" : _arrowDownImageView,
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
    AddSameCenterYConstraint(self, _arrowDownImageView);
    ApplyVisualConstraintsWithMetrics(constraints, views, metrics);

#if defined(__IPHONE_13_4)
    if (@available(iOS 13.4, *)) {
        [self addInteraction:[[ViewPointerInteraction alloc] init]];
    }
#endif  // defined(__IPHONE_13_4)

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

#pragma mark - Private

- (CGPoint)locationFromTouches:(NSSet*)touches {
  UITouch* touch = [touches anyObject];
  return [touch locationInView:self];
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
