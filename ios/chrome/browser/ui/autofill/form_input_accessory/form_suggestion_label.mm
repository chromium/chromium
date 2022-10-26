// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_suggestion_label.h"

#import <QuartzCore/QuartzCore.h>
#import <stddef.h>
#import <cmath>

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_data_util.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/form_suggestion_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Font size of button titles.
const CGFloat kIpadFontSize = 15.0f;
const CGFloat kIphoneFontSize = 14.0f;

// The horizontal space between the edge of the background and the text.
const CGFloat kBorderWidth = 14.0f;
// The space between items in the label.
const CGFloat kSpacing = 4.0f;

// Duration of animation transition.
const NSTimeInterval animationTransitionDuration = 0.5;
// Duration of animation effect.
const NSTimeInterval animationOnScreenDuration = 3.0;

// Structure that record the image for each icon.
struct IconImageMap {
  const char* const icon_name;
  NSString* image_name;
};

// Creates a label with the given `text` and `alpha` suitable for use in a
// suggestion button in the keyboard accessory view.
UILabel* TextLabel(NSString* text, UIColor* textColor, BOOL bold) {
  UILabel* label = [[UILabel alloc] init];
  [label setText:text];
  CGFloat fontSize =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? kIpadFontSize
          : kIphoneFontSize;
  UIFont* font = bold ? [UIFont boldSystemFontOfSize:fontSize]
                      : [UIFont systemFontOfSize:fontSize];
  [label setFont:font];
  label.textColor = textColor;
  [label setBackgroundColor:[UIColor clearColor]];
  return label;
}

}  // namespace

@interface FormSuggestionLabel ()

@property(strong, nonatomic) UILabel* suggestionLabel;
@property(strong, nonatomic) UILabel* descriptionLabel;
@property(nonatomic, getter=isHighlighted) BOOL highlighted;

@end

@implementation FormSuggestionLabel {
  // Client of this view.
  __weak id<FormSuggestionLabelDelegate> _delegate;
  FormSuggestion* _suggestion;
}

#pragma mark - Public

- (id)initWithSuggestion:(FormSuggestion*)suggestion
                   index:(NSUInteger)index
          numSuggestions:(NSUInteger)numSuggestions
                delegate:(id<FormSuggestionLabelDelegate>)delegate {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _suggestion = suggestion;
    _delegate = delegate;

    UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.alignment = UIStackViewAlignmentCenter;
    stackView.layoutMarginsRelativeArrangement = YES;
    stackView.layoutMargins =
        UIEdgeInsetsMake(0, kBorderWidth, 0, kBorderWidth);
    stackView.spacing = kSpacing;
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:stackView];
    AddSameConstraints(stackView, self);

    if (suggestion.icon.length > 0) {
      const int iconImageID = autofill::data_util::GetPaymentRequestData(
                                  base::SysNSStringToUTF8(suggestion.icon))
                                  .icon_resource_id;
      UIImage* iconImage = NativeImage(iconImageID);
      UIImageView* iconView = [[UIImageView alloc] initWithImage:iconImage];
      [stackView addArrangedSubview:iconView];
    }

    UILabel* label = TextLabel(suggestion.value,
                               [UIColor colorNamed:kTextPrimaryColor], YES);
    [label setHighlightedTextColor:[UIColor colorNamed:kBlue700Color]];
    [stackView addArrangedSubview:label];
    if ([suggestion.displayDescription length] > 0) {
      UILabel* description =
          TextLabel(suggestion.displayDescription,
                    [UIColor colorNamed:kTextSecondaryColor], NO);
      [stackView addArrangedSubview:description];
      self.descriptionLabel = description;
    }
    self.suggestionLabel = label;

    [self setHighlighted:NO];

    [self setClipsToBounds:YES];
    [self setUserInteractionEnabled:YES];
    [self setIsAccessibilityElement:YES];
    [self setAccessibilityLabel:l10n_util::GetNSStringF(
                                    IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION,
                                    base::SysNSStringToUTF16(suggestion.value),
                                    base::SysNSStringToUTF16(
                                        suggestion.displayDescription),
                                    base::NumberToString16(index + 1),
                                    base::NumberToString16(numSuggestions))];
    [self
        setAccessibilityIdentifier:kFormSuggestionLabelAccessibilityIdentifier];
  }

  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius = self.bounds.size.height / 2.0;
}

// Animates `highlight` property to YES for a duration of
// `animationOnScreenDuration`.
- (void)animateWithHighlight {
  __weak __typeof(self) weakSelf = self;
  [self animateWithHighlight:YES
                  completion:^(BOOL finished) {
                    if (finished) {
                      dispatch_after(
                          dispatch_time(DISPATCH_TIME_NOW,
                                        (int64_t)(animationOnScreenDuration *
                                                  NSEC_PER_SEC)),
                          dispatch_get_main_queue(), ^{
                            [weakSelf animateWithHighlight:NO completion:nil];
                          });
                    } else {
                      weakSelf.highlighted = NO;
                    }
                  }];
}

#pragma mark - Private

// Animates `highlight` property from current state to `highlighted`.
- (void)animateWithHighlight:(BOOL)highlighted
                  completion:(void (^)(BOOL))completion {
  if (self.highlighted == highlighted) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  [UIView transitionWithView:weakSelf
                    duration:animationTransitionDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    weakSelf.highlighted = highlighted;
                  }
                  completion:completion];
}

#pragma mark - Property

- (void)setHighlighted:(BOOL)highlighted {
  _highlighted = highlighted;
  self.suggestionLabel.highlighted = highlighted;
  self.descriptionLabel.highlighted = highlighted;
  self.backgroundColor =
      highlighted ? [UIColor colorNamed:kTextfieldHighlightBackgroundColor]
                  : [UIColor colorNamed:kGrey100Color];
}

#pragma mark - UIResponder

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  self.highlighted = NO;
  [self setBackgroundColor:[UIColor colorNamed:kGrey300Color]];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  // No op. This method implementation is needed per the UIResponder docs.
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  self.highlighted = NO;
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  self.highlighted = NO;

  // Don't count touches ending outside the view as as taps.
  CGPoint locationInView = [touches.anyObject locationInView:self];
  if (CGRectContainsPoint(self.bounds, locationInView)) {
    [_delegate didTapFormSuggestionLabel:self];
  }
}

@end
