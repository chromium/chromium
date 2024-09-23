// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/views/identity_view.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

const CGFloat kIdentityButtonControlRadius = 8.;
// Sizes.
const CGFloat kArrowDownSize = 24.;
// Distances/margins.
const CGFloat kArrowDownMargin = 12.;

}  // namespace

@interface IdentityButtonControl ()

@property(nonatomic, strong) IdentityView* identityView;
// Image View for the arrow (down or left according to `style`, see the
// `arrowDirection` property), letting the user know that more profiles can be
// selected.
@property(nonatomic, strong) UIImageView* arrowImageView;

@end

@implementation IdentityButtonControl

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.accessibilityIdentifier = kIdentityButtonControlIdentifier;
    self.layer.cornerRadius = kIdentityButtonControlRadius;
    [self setUnhighlightedBackgroundColor];

    // Adding view elements inside.
    // Down or right arrow.
    _arrowImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _arrowImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _arrowDirection = IdentityButtonControlArrowDown;
    [self updateArrowDirection];
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

    [self addInteraction:[[ViewPointerInteraction alloc] init]];

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

- (void)setIdentityViewStyle:(IdentityViewStyle)style {
  self.identityView.style = style;
  if (!self.highlighted)
    [self setUnhighlightedBackgroundColor];
}

- (IdentityViewStyle)identityViewStyle {
  return self.identityView.style;
}

#pragma mark - Private

// Sets the background color when not highlighted.
- (void)setUnhighlightedBackgroundColor {
  DCHECK(!self.highlighted);
  switch (self.identityView.style) {
    case IdentityViewStyleDefault:
    case IdentityViewStyleIdentityChooser:
      self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
      break;
    case IdentityViewStyleConsistency:
      self.backgroundColor =
          [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
      break;
  }
}

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
      image = [image imageFlippedForRightToLeftLayoutDirection];
      tintColor = [UIColor colorNamed:kTextTertiaryColor];
      break;
    case IdentityButtonControlArrowDown:
      image = [UIImage imageNamed:@"identity_picker_view_arrow_down"];
      tintColor = [UIColor colorNamed:kTextQuaternaryColor];
      break;
  }
  DCHECK(image);
  self.arrowImageView.image =
      [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  self.arrowImageView.tintColor = tintColor;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];

  if (highlighted) {
    self.backgroundColor = [UIColor colorNamed:kGrey300Color];
  } else {
    [self setUnhighlightedBackgroundColor];
  }
}

@end
