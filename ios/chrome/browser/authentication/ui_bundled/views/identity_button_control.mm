// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/views/identity_button_control.h"

#import "base/check.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/views/identity_view.h"
#import "ios/chrome/browser/authentication/ui_bundled/views/views_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

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

@implementation IdentityButtonControl {
  // Trailing constraint from `_identityView` to `_arrowImageView`, active when
  // `interactive` is `YES`.
  NSLayoutConstraint* _withArrowConstraint;
  // Trailing constraint from `_identityView` to `self`, active when
  // `interactive` is `NO`.
  NSLayoutConstraint* _noArrowConstraint;
  // Account name passed to `-setIdentityName:email:managed:`, or `nil`.
  NSString* _identityName;
  // Account email passed to `-setIdentityName:email:managed:`, or `nil` before
  // `-setIdentityName:email:managed:` is called.
  NSString* _identityEmail;
  // Whether the identity passed to `-setIdentityName:email:managed:` is
  // managed.
  BOOL _identityManaged;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _interactive = YES;
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
    _withArrowConstraint = [_identityView.trailingAnchor
        constraintEqualToAnchor:_arrowImageView.leadingAnchor
                       constant:-kArrowDownMargin];
    _noArrowConstraint = [_identityView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kArrowDownMargin];

    [NSLayoutConstraint activateConstraints:@[
      [_identityView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [_identityView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_identityView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [_arrowImageView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kArrowDownMargin],
      [_arrowImageView.widthAnchor constraintEqualToConstant:kArrowDownSize],
      [_arrowImageView.heightAnchor constraintEqualToConstant:kArrowDownSize],
      // By default, `_withArrowConstraint` is active (`_interactive` is `YES`).
      _withArrowConstraint,
    ]];
    AddSameCenterYConstraint(self, _arrowImageView);

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

- (void)setIdentityName:(NSString*)name
                  email:(NSString*)email
                managed:(BOOL)managed {
  CHECK(email, base::NotFatalUntil::M170);
  if (!email) {
    return;
  }
  _identityName = [name copy];
  _identityEmail = [email copy];
  _identityManaged = managed;
  if (!name.length) {
    [self.identityView setTitle:email subtitle:nil managed:managed];
  } else {
    [self.identityView setTitle:name subtitle:email managed:managed];
  }
  [self updateAccessibility];
}

#pragma mark - Properties

- (void)setInteractive:(BOOL)interactive {
  if (_interactive == interactive) {
    return;
  }
  _interactive = interactive;
  _arrowImageView.hidden = !interactive;
  _withArrowConstraint.active = interactive;
  _noArrowConstraint.active = !interactive;
  self.userInteractionEnabled = interactive;
  self.accessibilityTraits =
      interactive ? UIAccessibilityTraitButton : UIAccessibilityTraitNone;
  if (_identityEmail) {
    [self updateAccessibility];
  }
}

- (void)setArrowDirection:(IdentityButtonControlArrowDirection)arrowDirection {
  _arrowDirection = arrowDirection;
  [self updateArrowDirection];
}

- (void)setIdentityViewStyle:(IdentityViewStyle)style {
  self.identityView.style = style;
  if (!self.highlighted) {
    [self setUnhighlightedBackgroundColor];
  }
}

- (IdentityViewStyle)identityViewStyle {
  return self.identityView.style;
}

#pragma mark - Private

// Updates `accessibilityLabel` from the configured identity and `interactive`.
- (void)updateAccessibility {
  CHECK(_identityEmail);
  if (!_identityName.length) {
    if (!self.interactive) {
      self.accessibilityLabel =
          _identityManaged
              ? l10n_util::GetNSStringF(
                    IDS_IOS_SIGNIN_ACCOUNT_PICKER_CHOOSE_ACCOUNT_ITEM_DESCRIPTION_WITH_EMAIL_MANAGED,
                    base::SysNSStringToUTF16(_identityEmail))
              : _identityEmail;
    } else {
      self.accessibilityLabel = l10n_util::GetNSStringF(
          _identityManaged
              ? IDS_IOS_SIGNIN_ACCOUNT_PICKER_DESCRIPTION_WITH_EMAIL_MANAGED
              : IDS_IOS_SIGNIN_ACCOUNT_PICKER_DESCRIPTION_WITH_EMAIL,
          base::SysNSStringToUTF16(_identityEmail));
    }
    return;
  }
  // Name is non-empty.
  if (!self.interactive) {
    self.accessibilityLabel = l10n_util::GetNSStringF(
        _identityManaged
            ? IDS_IOS_SIGNIN_ACCOUNT_PICKER_CHOOSE_ACCOUNT_ITEM_DESCRIPTION_WITH_NAME_AND_EMAIL_MANAGED
            : IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME,
        base::SysNSStringToUTF16(_identityName),
        base::SysNSStringToUTF16(_identityEmail));
    return;
  }
  self.accessibilityLabel = l10n_util::GetNSStringF(
      _identityManaged
          ? IDS_IOS_SIGNIN_ACCOUNT_PICKER_DESCRIPTION_WITH_NAME_AND_EMAIL_MANAGED
          : IDS_IOS_SIGNIN_ACCOUNT_PICKER_DESCRIPTION_WITH_NAME_AND_EMAIL,
      base::SysNSStringToUTF16(_identityName),
      base::SysNSStringToUTF16(_identityEmail));
}

// Sets the background color when not highlighted.
- (void)setUnhighlightedBackgroundColor {
  DCHECK(!self.highlighted);
  switch (self.identityView.style) {
    case IdentityViewStyleDefault:
    case IdentityViewStyleIdentityChooser:
      self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
      break;
    case IdentityViewStyleConsistency:
    case IdentityViewStyleConsistencyContained:
    case IdentityViewStyleConsistencyDefaultIdentity:
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
