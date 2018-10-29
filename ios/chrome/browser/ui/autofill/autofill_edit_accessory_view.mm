// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_edit_accessory_view.h"

#include <string>

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ui/base/l10n/l10n_util.h"

NSString* const kAutofillEditAccessoryViewAccessibilityID =
    @"kAutofillEditAccessoryViewAccessibilityID";

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kDefaultAccessoryHeight = 44;
const CGRect kDefaultAccessoryFrame = {{0, 0}, {0, kDefaultAccessoryHeight}};

const CGFloat kDefaultAccessoryButtonWidth = kDefaultAccessoryHeight;
const CGRect kDefaultAccessoryButtonRect = {
    {0, 0},
    {kDefaultAccessoryButtonWidth, kDefaultAccessoryHeight}};

const CGFloat kDefaultAccessorySeparatorWidth = 1;
const CGFloat kDefaultAccessorySeparatorHeight = kDefaultAccessoryHeight;
const CGRect kDefaultAccessorySeparatorRect = {
    {0, 0},
    {kDefaultAccessorySeparatorWidth, kDefaultAccessorySeparatorHeight}};

UIImage* ButtonImage(NSString* name) {
  UIImage* rawImage = [UIImage imageNamed:name];
  return StretchableImageFromUIImage(rawImage, 1, 0);
}

UIImageView* ImageViewWithImageName(NSString* imageName) {
  UIImage* image =
      StretchableImageFromUIImage([UIImage imageNamed:imageName], 0, 0);

  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  [imageView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [imageView setFrame:kDefaultAccessorySeparatorRect];

  return imageView;
}

}  // namespace

@interface AutofillEditAccessoryView () {
  UIButton* _previousButton;
  UIButton* _nextButton;
  __weak id<AutofillEditAccessoryDelegate> _delegate;
}
@end

@implementation AutofillEditAccessoryView

- (UIButton*)previousButton {
  return _previousButton;
}

- (UIButton*)nextButton {
  return _nextButton;
}

- (instancetype)initWithDelegate:(id<AutofillEditAccessoryDelegate>)delegate {
  self = [super initWithFrame:kDefaultAccessoryFrame];
  if (!self)
    return nil;

  self.accessibilityIdentifier = kAutofillEditAccessoryViewAccessibilityID;

  _delegate = delegate;
  [self addBackgroundImage];
  [self setupSubviews];

  return self;
}

- (void)setupSubviews {
  _previousButton =
      [self accessoryButtonWithNormalName:@"autofill_prev"
                              pressedName:@"autofill_prev_pressed"
                             disabledName:@"autofill_prev_inactive"
                                   action:@selector(previousPressed)];
  [_previousButton
      setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_PREVIOUS)];
  [self addSubview:_previousButton];

  UIImageView* firstSeparator = ImageViewWithImageName(@"autofill_middle_sep");
  [self addSubview:firstSeparator];

  _nextButton = [self accessoryButtonWithNormalName:@"autofill_next"
                                        pressedName:@"autofill_next_pressed"
                                       disabledName:@"autofill_next_inactive"
                                             action:@selector(nextPressed)];
  [_nextButton setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_NEXT)];
  [self addSubview:_nextButton];

  UIImageView* secondSeparator = nil;
  UIButton* closeButton = nil;
  if (!IsIPadIdiom()) {
    closeButton = [self accessoryButtonWithNormalName:@"autofill_close"
                                          pressedName:@"autofill_close_pressed"
                                         disabledName:nil
                                               action:@selector(closePressed)];
    [closeButton
        setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_CLOSE)];
    [self addSubview:closeButton];

    secondSeparator = ImageViewWithImageName(@"autofill_middle_sep");
    [self addSubview:secondSeparator];
  }

  NSDictionary* bindings = NSDictionaryOfVariableBindings(
      _previousButton, firstSeparator, _nextButton);
  NSString* horizontalLayout =
      @"H:[_previousButton][firstSeparator][_nextButton]|";
  if (closeButton) {
    bindings = NSDictionaryOfVariableBindings(_previousButton, firstSeparator,
                                              _nextButton, secondSeparator,
                                              closeButton);
    horizontalLayout = @"H:[_previousButton][firstSeparator][_nextButton]["
                       @"secondSeparator][closeButton]|";
  }

  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:horizontalLayout
                                               options:0
                                               metrics:0
                                                 views:bindings]];
  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"V:[_previousButton]|"
                                               options:0
                                               metrics:0
                                                 views:bindings]];
  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"V:[firstSeparator]|"
                                               options:0
                                               metrics:0
                                                 views:bindings]];
  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"V:[_nextButton]|"
                                               options:0
                                               metrics:nil
                                                 views:bindings]];

  if (closeButton) {
    [self addConstraints:[NSLayoutConstraint
                             constraintsWithVisualFormat:@"V:[secondSeparator]|"
                                                 options:0
                                                 metrics:nil
                                                   views:bindings]];
    [self addConstraints:[NSLayoutConstraint
                             constraintsWithVisualFormat:@"V:[closeButton]|"
                                                 options:0
                                                 metrics:nil
                                                   views:bindings]];
  }
}

- (UIButton*)accessoryButtonWithNormalName:(NSString*)normalName
                               pressedName:(NSString*)pressedName
                              disabledName:(NSString*)disabledName
                                    action:(SEL)action {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];

  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  [button setFrame:kDefaultAccessoryButtonRect];

  [button setBackgroundImage:ButtonImage(normalName)
                    forState:UIControlStateNormal];
  [button setBackgroundImage:ButtonImage(pressedName)
                    forState:UIControlStateSelected];
  [button setBackgroundImage:ButtonImage(pressedName)
                    forState:UIControlStateHighlighted];

  if (disabledName) {
    [button setBackgroundImage:ButtonImage(disabledName)
                      forState:UIControlStateDisabled];
  }

  [button addTarget:_delegate
                action:action
      forControlEvents:UIControlEventTouchUpInside];

  return button;
}

- (void)addBackgroundImage {
  UIImage* backgroundImage =
      StretchableImageNamed(@"autofill_keyboard_background");

  UIImageView* backgroundImageView =
      [[UIImageView alloc] initWithFrame:[self bounds]];
  [backgroundImageView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [backgroundImageView setImage:backgroundImage];
  [self insertSubview:backgroundImageView atIndex:0];
}

@end
