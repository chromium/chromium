// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_picker_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_view.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Ink/src/MaterialInk.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kIdentityPickerViewIdentifier =
    @"kIdentityPickerViewIdentifier";

namespace {

const CGFloat kIdentityPickerViewRadius = 8.;
// Sizes.
const CGFloat kArrowDownSize = 24.;
// Distances/margins.
const CGFloat kArrowDownMargin = 12.;
const CGFloat kHorizontalAvatarMargin = 16.;
const CGFloat kVerticalMargin = 12.;
// Colors
const int kHeaderBackgroundColor = 0xf1f3f4;

}  // namespace

@interface IdentityPickerView ()

@property(nonatomic, strong) IdentityView* identityView;
@property(nonatomic, strong) MDCInkView* inkView;
// Image View for the down arrow, letting the user know that more profiles can
// be selected.
@property(nonatomic, strong) UIImageView* arrowDownImageView;
// Image View for the checkmark, indicating that the selected identity cannot be
// changed.
@property(nonatomic, strong) UIImageView* checkmarkImageView;

@end

@implementation IdentityPickerView

@synthesize canChangeIdentity = _canChangeIdentity;
@synthesize identityView = _identityView;
@synthesize inkView = _inkView;
@synthesize arrowDownImageView = _arrowDownImageView;
@synthesize checkmarkImageView = _checkmarkImageView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.accessibilityIdentifier = kIdentityPickerViewIdentifier;
    self.layer.cornerRadius = kIdentityPickerViewRadius;
    self.backgroundColor = UIColorFromRGB(kHeaderBackgroundColor);
    // Adding view elements inside.
    // Ink view.
    _inkView = [[MDCInkView alloc] initWithFrame:CGRectZero];
    _inkView.layer.cornerRadius = kIdentityPickerViewRadius;
    _inkView.inkStyle = MDCInkStyleBounded;
    _inkView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_inkView];

    // Arrow down.
    _arrowDownImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _arrowDownImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _arrowDownImageView.image =
        [UIImage imageNamed:@"identity_picker_view_arrow_down"];
    [self addSubview:_arrowDownImageView];

    // Checkmark.
    _checkmarkImageView = [[UIImageView alloc] init];
    _checkmarkImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _checkmarkImageView.image =
        [[UIImage imageNamed:kAuthenticationCheckmarkImageName]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _checkmarkImageView.tintColor =
        UIColorFromRGB(kAuthenticationCheckmarkColor);
    [self addSubview:_checkmarkImageView];

    // Main view with avatar, name and email.
    _identityView = [[IdentityView alloc] initWithFrame:CGRectZero];
    _identityView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_identityView];

    // Layout constraints.
    NSDictionary* views = @{
      @"identityview" : _identityView,
      @"arrow" : _arrowDownImageView,
      @"checkmark" : _checkmarkImageView,
    };
    NSDictionary* metrics = @{
      @"ArrowMargin" : @(kArrowDownMargin),
      @"ArrowSize" : @(kArrowDownSize),
      @"CheckmarkSize" : @(kAuthenticationCheckmarkSize),
      @"HAvatMrg" : @(kHorizontalAvatarMargin),
      @"VMargin" : @(kVerticalMargin),
    };
    NSArray* constraints = @[
      // Horizontal constraints.
      @"H:|[identityview]-(ArrowMargin)-[arrow]-(ArrowMargin)-|",
      // Vertical constraints.
      @"V:|[identityview]|",
      // Size constraints.
      @"H:[arrow(ArrowSize)]",
      @"V:[arrow(ArrowSize)]",
      @"H:[checkmark(CheckmarkSize)]",
      @"V:[checkmark(CheckmarkSize)]",
    ];
    AddSameCenterYConstraint(self, _arrowDownImageView);
    AddSameCenterConstraints(_checkmarkImageView, _arrowDownImageView);
    ApplyVisualConstraintsWithMetrics(constraints, views, metrics);

    // Accessibility.
    self.isAccessibilityElement = YES;
    self.accessibilityTraits = UIAccessibilityTraitButton;
  }
  return self;
}

#pragma mark - Setter

- (void)setCanChangeIdentity:(BOOL)canChangeIdentity {
  _canChangeIdentity = canChangeIdentity;
  self.enabled = canChangeIdentity;
  self.arrowDownImageView.hidden = !canChangeIdentity;
  self.checkmarkImageView.hidden = canChangeIdentity;
  self.accessibilityTraits = canChangeIdentity ? UIAccessibilityTraitButton
                                               : UIAccessibilityTraitStaticText;
}

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
  [self.inkView startTouchBeganAnimationAtPoint:location completion:nil];
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesEnded:touches withEvent:event];
  CGPoint location = [self locationFromTouches:touches];
  [self.inkView startTouchEndedAnimationAtPoint:location completion:nil];
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesCancelled:touches withEvent:event];
  CGPoint location = [self locationFromTouches:touches];
  [self.inkView startTouchEndedAnimationAtPoint:location completion:nil];
}

@end
