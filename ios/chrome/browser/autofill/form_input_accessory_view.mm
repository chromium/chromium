// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory_view.h"

#import <QuartzCore/QuartzCore.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The alpha value of the background color.
const CGFloat kBackgroundColorAlpha = 1.0;

// The width of the separators of the previous and next buttons.
const CGFloat kNavigationButtonSeparatorWidth = 1;

// The width of the shadow part of the navigation area separator.
const CGFloat kNavigationAreaSeparatorShadowWidth = 2;

// The width of the navigation area / custom view separator asset.
const CGFloat kNavigationAreaSeparatorWidth = 1;

// The width for the white gradient UIImageView.
constexpr CGFloat ManualFillGradientWidth = 44;

// The margin for the white gradient UIImageView.
constexpr CGFloat ManualFillGradientMargin = 14;

// The spacing between the items in the navigation view.
constexpr CGFloat ManualFillNavigationItemSpacing = 4;

// The left content inset for the close button.
constexpr CGFloat ManualFillCloseButtonLeftInset = 7;

// The right content inset for the close button.
constexpr CGFloat ManualFillCloseButtonRightInset = 15;

// The height for the top and bottom sepparator lines.
constexpr CGFloat ManualFillSeparatorHeight = 0.5;

}  // namespace

@interface FormInputAccessoryView ()

// Returns a view that shows navigation buttons.
- (UIView*)viewForNavigationButtonsUsingDelegate:
    (id<FormInputAccessoryViewDelegate>)delegate;

// Adds a navigation button for Autofill in |view| that has |normalImage| for
// state UIControlStateNormal, a |pressedImage| for states
// UIControlStateSelected and UIControlStateHighlighted, and an optional
// |disabledImage| for UIControlStateDisabled.
- (UIButton*)addKeyboardNavButtonWithNormalImage:(UIImage*)normalImage
                                    pressedImage:(UIImage*)pressedImage
                                   disabledImage:(UIImage*)disabledImage
                                          target:(id)target
                                          action:(SEL)action
                                         enabled:(BOOL)enabled
                                          inView:(UIView*)view;

// Adds a background image to |view|. The supplied image is stretched to fit the
// space by stretching the content its horizontal and vertical centers.
+ (void)addBackgroundImageInView:(UIView*)view
                   withImageName:(NSString*)imageName;

// Adds an image view in |view| with an image named |imageName|.
+ (UIView*)createImageViewWithImageName:(NSString*)imageName
                                 inView:(UIView*)view;

@end

@implementation FormInputAccessoryView

- (void)setUpWithCustomView:(UIView*)customView {
  [self addSubview:customView];
  customView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self, customView);

  [[self class] addBackgroundImageInView:self
                           withImageName:@"autofill_keyboard_background"];
}

- (void)setUpWithNavigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate
                         customView:(UIView*)customView {
  self.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* customViewContainer = [[UIView alloc] init];
  customViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:customViewContainer];

  [customViewContainer addSubview:customView];
  customView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(customViewContainer, customView);

  UIView* navigationView =
      [self viewForNavigationButtonsUsingDelegate:delegate];
  navigationView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:navigationView];

  id<LayoutGuideProvider> layoutGuide = SafeAreaLayoutGuideForView(self);
  [NSLayoutConstraint activateConstraints:@[
    [customViewContainer.topAnchor
        constraintEqualToAnchor:layoutGuide.topAnchor],
    [customViewContainer.bottomAnchor
        constraintEqualToAnchor:layoutGuide.bottomAnchor],
    [customViewContainer.leadingAnchor
        constraintEqualToAnchor:layoutGuide.leadingAnchor],
    [navigationView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor],
    [navigationView.topAnchor constraintEqualToAnchor:layoutGuide.topAnchor],
    [navigationView.bottomAnchor
        constraintEqualToAnchor:layoutGuide.bottomAnchor],
  ]];

  if (autofill::features::IsPasswordManualFallbackEnabled()) {
    self.backgroundColor = UIColor.whiteColor;

    UIImage* gradientImage = [[UIImage imageNamed:@"mf_gradient"]
        resizableImageWithCapInsets:UIEdgeInsetsZero
                       resizingMode:UIImageResizingModeStretch];
    UIImageView* gradientView =
        [[UIImageView alloc] initWithImage:gradientImage];
    gradientView.translatesAutoresizingMaskIntoConstraints = NO;
    [self insertSubview:gradientView belowSubview:navigationView];

    UIView* topGrayLine = [[UIView alloc] init];
    topGrayLine.backgroundColor = UIColor.cr_manualFillSeparatorColor;
    topGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:topGrayLine];

    UIView* bottomGrayLine = [[UIView alloc] init];
    bottomGrayLine.backgroundColor = UIColor.cr_manualFillSeparatorColor;
    bottomGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:bottomGrayLine];

    [NSLayoutConstraint activateConstraints:@[
      [topGrayLine.topAnchor constraintEqualToAnchor:self.topAnchor],
      [topGrayLine.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [topGrayLine.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [topGrayLine.heightAnchor
          constraintEqualToConstant:ManualFillSeparatorHeight],

      [bottomGrayLine.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [bottomGrayLine.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [bottomGrayLine.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [bottomGrayLine.heightAnchor
          constraintEqualToConstant:ManualFillSeparatorHeight],

      [gradientView.topAnchor constraintEqualToAnchor:navigationView.topAnchor],
      [gradientView.bottomAnchor
          constraintEqualToAnchor:navigationView.bottomAnchor],
      [gradientView.widthAnchor
          constraintEqualToConstant:ManualFillGradientWidth],
      [gradientView.trailingAnchor
          constraintEqualToAnchor:navigationView.leadingAnchor
                         constant:ManualFillGradientMargin],

      [customViewContainer.trailingAnchor
          constraintEqualToAnchor:navigationView.leadingAnchor],
    ]];
  } else {
    [[self class] addBackgroundImageInView:self
                             withImageName:@"autofill_keyboard_background"];
    [customViewContainer.trailingAnchor
        constraintEqualToAnchor:navigationView.leadingAnchor
                       constant:kNavigationAreaSeparatorShadowWidth]
        .active = YES;
  }
}

#pragma mark -
#pragma mark UIInputViewAudioFeedback

- (BOOL)enableInputClicksWhenVisible {
  return YES;
}

#pragma mark -
#pragma mark Private Methods

UIImage* ButtonImage(NSString* name) {
  UIImage* rawImage = [UIImage imageNamed:name];
  return StretchableImageFromUIImage(rawImage, 1, 0);
}

- (UIView*)viewForNavigationButtonsUsingDelegate:
    (id<FormInputAccessoryViewDelegate>)delegate {
  if (autofill::features::IsPasswordManualFallbackEnabled()) {
    UIButton* previousButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [previousButton setImage:[UIImage imageNamed:@"mf_arrow_up"]
                    forState:UIControlStateNormal];
    previousButton.tintColor = UIColor.cr_manualFillTintColor;
    [previousButton addTarget:delegate
                       action:@selector(selectPreviousElementWithButtonPress)
             forControlEvents:UIControlEventTouchUpInside];
    previousButton.enabled = NO;
    NSString* previousButtonAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD);
    [previousButton setAccessibilityLabel:previousButtonAccessibilityLabel];

    UIButton* nextButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [nextButton setImage:[UIImage imageNamed:@"mf_arrow_down"]
                forState:UIControlStateNormal];
    nextButton.tintColor = UIColor.cr_manualFillTintColor;
    [nextButton addTarget:delegate
                   action:@selector(selectNextElementWithButtonPress)
         forControlEvents:UIControlEventTouchUpInside];
    nextButton.enabled = NO;
    NSString* nextButtonAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD);
    [nextButton setAccessibilityLabel:nextButtonAccessibilityLabel];

    UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
    NSString* title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_INPUT_BAR_DONE);
    [closeButton setTitle:title forState:UIControlStateNormal];
    closeButton.tintColor = UIColor.cr_manualFillTintColor;
    [closeButton addTarget:delegate
                    action:@selector(closeKeyboardWithButtonPress)
          forControlEvents:UIControlEventTouchUpInside];
    closeButton.contentEdgeInsets = UIEdgeInsetsMake(
        0, ManualFillCloseButtonLeftInset, 0, ManualFillCloseButtonRightInset);
    NSString* closeButtonAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD);
    [closeButton setAccessibilityLabel:closeButtonAccessibilityLabel];

    [delegate fetchPreviousAndNextElementsPresenceWithCompletionHandler:^(
                  BOOL hasPreviousElement, BOOL hasNextElement) {
      previousButton.enabled = hasPreviousElement;
      nextButton.enabled = hasNextElement;
    }];

    UIStackView* navigationView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ previousButton, nextButton, closeButton ]];
    navigationView.spacing = ManualFillNavigationItemSpacing;
    return navigationView;
  }

  UIView* navView = [[UIView alloc] init];
  navView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* separator =
      [[self class] createImageViewWithImageName:@"autofill_left_sep"
                                          inView:navView];

  UIButton* previousButton = [self
      addKeyboardNavButtonWithNormalImage:ButtonImage(@"autofill_prev")
                             pressedImage:ButtonImage(@"autofill_prev_pressed")
                            disabledImage:ButtonImage(@"autofill_prev_inactive")
                                   target:delegate
                                   action:@selector
                                   (selectPreviousElementWithButtonPress)
                                  enabled:NO
                                   inView:navView];
  [previousButton
      setAccessibilityLabel:l10n_util::GetNSString(
                                IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD)];

  // Add internal separator.
  UIView* internalSeparator =
      [[self class] createImageViewWithImageName:@"autofill_middle_sep"
                                          inView:navView];

  UIButton* nextButton = [self
      addKeyboardNavButtonWithNormalImage:ButtonImage(@"autofill_next")
                             pressedImage:ButtonImage(@"autofill_next_pressed")
                            disabledImage:ButtonImage(@"autofill_next_inactive")
                                   target:delegate
                                   action:@selector
                                   (selectNextElementWithButtonPress)
                                  enabled:NO
                                   inView:navView];
  [nextButton setAccessibilityLabel:l10n_util::GetNSString(
                                        IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD)];

  [delegate fetchPreviousAndNextElementsPresenceWithCompletionHandler:^(
                BOOL hasPreviousElement, BOOL hasNextElement) {
    previousButton.enabled = hasPreviousElement;
    nextButton.enabled = hasNextElement;
  }];

  // Add internal separator.
  UIView* internalSeparator2 =
      [[self class] createImageViewWithImageName:@"autofill_middle_sep"
                                          inView:navView];

  UIButton* closeButton = [self
      addKeyboardNavButtonWithNormalImage:ButtonImage(@"autofill_close")
                             pressedImage:ButtonImage(@"autofill_close_pressed")
                            disabledImage:nil
                                   target:delegate
                                   action:@selector
                                   (closeKeyboardWithButtonPress)
                                  enabled:YES
                                   inView:navView];
  [closeButton
      setAccessibilityLabel:l10n_util::GetNSString(
                                IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD)];

  ApplyVisualConstraintsWithMetrics(
      @[
        (@"H:|[separator1(==areaSeparatorWidth)][previousButton][separator2(=="
         @"buttonSeparatorWidth)][nextButton][internalSeparator2("
         @"buttonSeparatorWidth)][closeButton]|"),
        @"V:|-(topPadding)-[separator1]|",
        @"V:|-(topPadding)-[previousButton]|",
        @"V:|-(topPadding)-[previousButton]|",
        @"V:|-(topPadding)-[separator2]|", @"V:|-(topPadding)-[nextButton]|",
        @"V:|-(topPadding)-[internalSeparator2]|",
        @"V:|-(topPadding)-[closeButton]|"
      ],
      @{
        @"separator1" : separator,
        @"previousButton" : previousButton,
        @"separator2" : internalSeparator,
        @"nextButton" : nextButton,
        @"internalSeparator2" : internalSeparator2,
        @"closeButton" : closeButton
      },
      @{

        @"areaSeparatorWidth" : @(kNavigationAreaSeparatorWidth),
        @"buttonSeparatorWidth" : @(kNavigationButtonSeparatorWidth),
        @"topPadding" : @(1)
      });

  return navView;
}

- (UIButton*)addKeyboardNavButtonWithNormalImage:(UIImage*)normalImage
                                    pressedImage:(UIImage*)pressedImage
                                   disabledImage:(UIImage*)disabledImage
                                          target:(id)target
                                          action:(SEL)action
                                         enabled:(BOOL)enabled
                                          inView:(UIView*)view {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  [button setBackgroundImage:normalImage forState:UIControlStateNormal];
  [button setBackgroundImage:pressedImage forState:UIControlStateSelected];
  [button setBackgroundImage:pressedImage forState:UIControlStateHighlighted];
  if (disabledImage)
    [button setBackgroundImage:disabledImage forState:UIControlStateDisabled];

  CALayer* layer = [button layer];
  layer.borderWidth = 0;
  layer.borderColor = [[UIColor blackColor] CGColor];
  button.enabled = enabled;
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [button.heightAnchor constraintEqualToAnchor:button.widthAnchor].active = YES;
  [view addSubview:button];
  return button;
}

+ (void)addBackgroundImageInView:(UIView*)view
                   withImageName:(NSString*)imageName {
  UIImage* backgroundImage = StretchableImageNamed(imageName);

  UIImageView* backgroundImageView = [[UIImageView alloc] init];
  backgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [backgroundImageView setImage:backgroundImage];
  [backgroundImageView setAlpha:kBackgroundColorAlpha];
  [view addSubview:backgroundImageView];
  [view sendSubviewToBack:backgroundImageView];
  AddSameConstraints(view, backgroundImageView);
}

+ (UIView*)createImageViewWithImageName:(NSString*)imageName
                                 inView:(UIView*)view {
  UIImage* image =
      StretchableImageFromUIImage([UIImage imageNamed:imageName], 0, 0);
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:imageView];
  return imageView;
}

@end
