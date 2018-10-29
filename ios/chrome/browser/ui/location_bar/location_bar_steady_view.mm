// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/location_bar/extended_touch_target_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Length of the trailing button side.
const CGFloat kButtonSize = 24;

// Space between the location icon and the location label.
const CGFloat kLocationImageToLabelSpacing = -4.0;
// Minimal horizontal padding between the leading edge of the location bar and
// the content of the location bar.
const CGFloat kLocationBarLeadingPadding = 5.0;
// Trailing space between the button and the trailing edge of the location bar.
const CGFloat kButtonTrailingSpacing = 10;

}  // namespace

@interface LocationBarSteadyView ()

// The image view displaying the current location icon (i.e. http[s] status).
@property(nonatomic, strong) UIImageView* locationIconImageView;

// The view containing the location label, and (sometimes) the location image
// view.
@property(nonatomic, strong) UIView* locationContainerView;

// Constraints to hide the trailing button.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* hideButtonConstraints;

// Constraints to show the trailing button.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* showButtonConstraints;

// Constraints to hide the location image view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* hideLocationImageConstraints;

// Constraints to show the location image view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* showLocationImageConstraints;

// Elements to surface in accessibility.
@property(nonatomic, strong) NSMutableArray* accessibleElements;

@end

#pragma mark - LocationBarSteadyViewColorScheme

@implementation LocationBarSteadyViewColorScheme
@synthesize fontColor = _fontColor;
@synthesize placeholderColor = _placeholderColor;
@synthesize trailingButtonColor = _trailingButtonColor;

+ (instancetype)standardScheme {
  LocationBarSteadyViewColorScheme* scheme =
      [[LocationBarSteadyViewColorScheme alloc] init];

  scheme.fontColor = [UIColor colorWithWhite:0 alpha:0.7];
  scheme.placeholderColor = [UIColor colorWithWhite:0 alpha:0.3];
  scheme.trailingButtonColor = [UIColor colorWithWhite:0 alpha:0.7];

  return scheme;
}

+ (instancetype)incognitoScheme {
  LocationBarSteadyViewColorScheme* scheme =
      [[LocationBarSteadyViewColorScheme alloc] init];

  scheme.fontColor = [UIColor whiteColor];
  scheme.placeholderColor = [UIColor colorWithWhite:1 alpha:0.5];
  scheme.trailingButtonColor = [UIColor whiteColor];

  return scheme;
}

@end

#pragma mark - LocationBarSteadyButton

// Buttons with a darker background in highlighted state.
@interface LocationBarSteadyButton : UIButton
@end

@implementation LocationBarSteadyButton

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [UIView animateWithDuration:0.1
                        delay:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     CGFloat alpha = 0;
                     if (highlighted)
                       alpha += 0.07;
                     self.backgroundColor =
                         [UIColor colorWithWhite:0 alpha:alpha];
                   }
                   completion:nil];
}

@end

#pragma mark - LocationBarSteadyView

@implementation LocationBarSteadyView
@synthesize locationButton = _locationButton;
@synthesize locationLabel = _locationLabel;
@synthesize locationIconImageView = _locationIconImageView;
@synthesize trailingButton = _trailingButton;
@synthesize hideButtonConstraints = _hideButtonConstraints;
@synthesize showButtonConstraints = _showButtonConstraints;
@synthesize hideLocationImageConstraints = _hideLocationImageConstraints;
@synthesize showLocationImageConstraints = _showLocationImageConstraints;
@synthesize locationContainerView = _locationContainerView;
@synthesize securityLevelAccessibilityString =
    _securityLevelAccessibilityString;
@synthesize accessibleElements = _accessibleElements;

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _locationLabel = [[UILabel alloc] init];
    _locationIconImageView = [[UIImageView alloc] init];
    _locationIconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [_locationIconImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    SetA11yLabelAndUiAutomationName(
        _locationIconImageView,
        IDS_IOS_PAGE_INFO_SECURITY_BUTTON_ACCESSIBILITY_LABEL,
        @"Page Security Info");
    _locationIconImageView.isAccessibilityElement = YES;

    // Setup trailing button.
    _trailingButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
    _trailingButton.translatesAutoresizingMaskIntoConstraints = NO;

    // Setup label.
    _locationLabel.lineBreakMode = NSLineBreakByTruncatingHead;
    _locationLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_locationLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:UILayoutConstraintAxisVertical];

    // Container for location label and icon.
    _locationContainerView = [[UIView alloc] init];
    _locationContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _locationContainerView.userInteractionEnabled = NO;
    [_locationContainerView addSubview:_locationIconImageView];
    [_locationContainerView addSubview:_locationLabel];

    _showLocationImageConstraints = @[
      [_locationContainerView.leadingAnchor
          constraintEqualToAnchor:_locationIconImageView.leadingAnchor],
      [_locationIconImageView.trailingAnchor
          constraintEqualToAnchor:_locationLabel.leadingAnchor
                         constant:kLocationImageToLabelSpacing],
      [_locationLabel.trailingAnchor
          constraintEqualToAnchor:_locationContainerView.trailingAnchor],
      [_locationIconImageView.centerYAnchor
          constraintEqualToAnchor:_locationContainerView.centerYAnchor],
    ];

    _hideLocationImageConstraints = @[
      [_locationContainerView.leadingAnchor
          constraintEqualToAnchor:_locationLabel.leadingAnchor],
      [_locationLabel.trailingAnchor
          constraintEqualToAnchor:_locationContainerView.trailingAnchor],
    ];

    [NSLayoutConstraint activateConstraints:_showLocationImageConstraints];

    _locationButton = [[LocationBarSteadyButton alloc] init];
    _locationButton.translatesAutoresizingMaskIntoConstraints = NO;
    _locationButton.layer.cornerRadius = kAdaptiveLocationBarCornerRadius;
    [_locationButton addSubview:_trailingButton];
    [_locationButton addSubview:_locationContainerView];

    [self addSubview:_locationButton];

    ApplyVisualConstraints(
        @[ @"V:|[label]|", @"V:|[container]|" ],
        @{@"label" : _locationLabel, @"container" : _locationContainerView});

    AddSameConstraints(self, _locationButton);

    // Make the label gravitate towards the center of the view.
    NSLayoutConstraint* centerX = [_locationContainerView.centerXAnchor
        constraintEqualToAnchor:self.centerXAnchor];
    centerX.priority = UILayoutPriorityDefaultHigh;

    [NSLayoutConstraint activateConstraints:@[
      [_locationContainerView.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                      constant:kLocationBarLeadingPadding],
      [_trailingButton.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [_locationContainerView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [_trailingButton.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:_locationContainerView
                                                   .trailingAnchor],
      centerX,
    ]];

    // Setup hiding constraints.
    _hideButtonConstraints = @[
      [_trailingButton.widthAnchor constraintEqualToConstant:0],
      [_trailingButton.heightAnchor constraintEqualToConstant:0],
      [self.trailingButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor]
    ];

    // Setup and activate the show button constraints.
    _showButtonConstraints = @[
      [_trailingButton.widthAnchor constraintEqualToConstant:kButtonSize],
      [_trailingButton.heightAnchor constraintEqualToConstant:kButtonSize],
      [self.trailingButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kButtonTrailingSpacing],
    ];
    [NSLayoutConstraint activateConstraints:_showButtonConstraints];
  }

  // Setup accessibility.
  _trailingButton.isAccessibilityElement = YES;
  _locationButton.isAccessibilityElement = YES;
  _locationButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_LOCATION);

  _accessibleElements = [[NSMutableArray alloc] init];
  [_accessibleElements addObject:_locationButton];
  [_accessibleElements addObject:_trailingButton];

  // These two elements must remain accessible for egtests, but will not be
  // included in accessibility navigation as they are not added to the
  // accessibleElements array.
  _locationIconImageView.isAccessibilityElement = YES;
  _locationLabel.isAccessibilityElement = YES;

  [self updateAccessibility];

  return self;
}

- (void)setColorScheme:(LocationBarSteadyViewColorScheme*)colorScheme {
  self.trailingButton.tintColor = colorScheme.trailingButtonColor;
  self.locationLabel.textColor = colorScheme.fontColor;
  self.locationIconImageView.tintColor = colorScheme.fontColor;
}

- (void)setLocationImage:(UIImage*)locationImage {
  BOOL hadImage = self.locationIconImageView.image != nil;
  BOOL hasImage = locationImage != nil;
  self.locationIconImageView.image = locationImage;
  if (hadImage == hasImage) {
    return;
  }

  if (hasImage) {
    [self.locationContainerView addSubview:self.locationIconImageView];
    [NSLayoutConstraint
        deactivateConstraints:self.hideLocationImageConstraints];
    [NSLayoutConstraint activateConstraints:self.showLocationImageConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:self.showLocationImageConstraints];
    [NSLayoutConstraint activateConstraints:self.hideLocationImageConstraints];
    [self.locationIconImageView removeFromSuperview];
  }
}

- (void)hideButton:(BOOL)hidden {
  if (hidden) {
    [NSLayoutConstraint deactivateConstraints:self.showButtonConstraints];
    [NSLayoutConstraint activateConstraints:self.hideButtonConstraints];
    [self.accessibleElements removeObject:self.trailingButton];
  } else {
    [NSLayoutConstraint deactivateConstraints:self.hideButtonConstraints];
    [NSLayoutConstraint activateConstraints:self.showButtonConstraints];
    [self.accessibleElements addObject:self.trailingButton];
  }
}

- (void)setLocationLabelText:(NSString*)string {
  if ([self.locationLabel.text isEqualToString:string]) {
    return;
  }
  self.locationLabel.text = string;
  [self updateAccessibility];
}

- (void)setSecurityLevelAccessibilityString:(NSString*)string {
  if ([_securityLevelAccessibilityString isEqualToString:string]) {
    return;
  }
  _securityLevelAccessibilityString = [string copy];
  [self updateAccessibility];
}

#pragma mark - UIResponder

// This is needed for UIMenu
- (BOOL)canBecomeFirstResponder {
  return true;
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  self.locationLabel.font =
      [UIFont systemFontOfSize:kLocationBarSteadyFontSize];
}

#pragma mark - UIAccessibilityContainer

- (NSArray*)accessibilityElements {
  return self.accessibleElements;
}

- (NSInteger)accessibilityElementCount {
  return self.accessibleElements.count;
}

- (id)accessibilityElementAtIndex:(NSInteger)index {
  return self.accessibleElements[index];
}

- (NSInteger)indexOfAccessibilityElement:(id)element {
  return [self.accessibleElements indexOfObject:element];
}

#pragma mark - private

- (void)updateAccessibility {
  if (self.securityLevelAccessibilityString.length > 0) {
    self.locationButton.accessibilityValue =
        [NSString stringWithFormat:@"%@ %@", self.locationLabel.text,
                                   self.securityLevelAccessibilityString];
  } else {
    self.locationButton.accessibilityValue =
        [NSString stringWithFormat:@"%@", self.locationLabel.text];
  }
}

@end
