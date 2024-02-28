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
#import "components/password_manager/ios/shared_password_controller.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Font size of button titles.
const CGFloat kIpadFontSize = 15.0f;
const CGFloat kIphoneFontSize = 14.0f;

// The horizontal space between the edge of the background and the text.
const CGFloat kBorderWidth = 14.0f;
// The space between items in the label.
const CGFloat kSpacing = 4.0f;
// The corner radius of the label.
const CGFloat kCornerRadius = 8.0f;

// Shadow parameters.
const CGFloat kShadowRadius = 0.5;
const CGFloat kShadowVerticalOffset = 1.0;
const CGFloat kShadowOpacity = 1.0;

// The preferred minimum width of the icon shown on the label.
const CGFloat kSuggestionIconWidth = 40;

// Offset required to see half of the icon of the 2nd credit card suggestion
// when the first credit card suggestion is at maximum width. This number
// represents the width of the stack view minus the width of the first
// suggestion.
const CGFloat kHalfCreditCardIconOffset =
    2 * kBorderWidth + 2 * kSpacing + 0.5 * kSuggestionIconWidth;

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

@implementation FormSuggestionLabel {
  // Client of this view.
  __weak id<FormSuggestionLabelDelegate> _delegate;
  FormSuggestion* _suggestion;
}

#pragma mark - Public

- (id)initWithSuggestion:(FormSuggestion*)suggestion
                    index:(NSUInteger)index
           numSuggestions:(NSUInteger)numSuggestions
    accessoryTrailingView:(UIView*)accessoryTrailingView
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

    if (suggestion.icon != nil) {
      UIImage* icon = suggestion.icon;
      if (IsKeyboardAccessoryUpgradeEnabled()) {
        if (icon && (icon.size.width > 0) &&
            (icon.size.width < kSuggestionIconWidth)) {
          // For a simple image resize, we can keep the same underlying image
          // and only adjust the ratio.
          CGFloat ratio = icon.size.width / kSuggestionIconWidth;
          icon = [UIImage imageWithCGImage:[icon CGImage]
                                     scale:icon.scale * ratio
                               orientation:icon.imageOrientation];
        }
      }
      UIImageView* iconView = [[UIImageView alloc] initWithImage:icon];
      [stackView addArrangedSubview:iconView];
    }

    NSString* suggestionText = suggestion.value;
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      UIStackView* verticalStackView =
          [[UIStackView alloc] initWithArrangedSubviews:@[]];
      verticalStackView.axis = UILayoutConstraintAxisVertical;
      verticalStackView.alignment = UIStackViewAlignmentLeading;
      verticalStackView.layoutMarginsRelativeArrangement = YES;
      verticalStackView.layoutMargins = UIEdgeInsetsMake(0, kBorderWidth, 0, 0);
      [stackView addArrangedSubview:verticalStackView];

      // Insert the next subviews vertically instead of horizonatally.
      stackView = verticalStackView;

      if ([suggestionText hasSuffix:kPasswordFormSuggestionSuffix]) {
        suggestionText = [suggestionText
            substringToIndex:suggestionText.length -
                             kPasswordFormSuggestionSuffix.length];
      }
    }

    UILabel* valueLabel =
        TextLabel(suggestionText, [UIColor colorNamed:kTextPrimaryColor], YES);
    [stackView addArrangedSubview:valueLabel];

    if ([suggestion.minorValue length] > 0) {
      UILabel* minorValueLabel = TextLabel(
          suggestion.minorValue, [UIColor colorNamed:kTextPrimaryColor], YES);
      [stackView addArrangedSubview:minorValueLabel];
    }

    if ([suggestion.displayDescription length] > 0) {
      UILabel* description =
          TextLabel(suggestion.displayDescription,
                    [UIColor colorNamed:kTextSecondaryColor], NO);
      [stackView addArrangedSubview:description];
    }

    [self setBackgroundColor:[self customBackgroundColor]];

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

    if (IsKeyboardAccessoryUpgradeEnabled()) {
      CGFloat maximumWidth = [self maximumWidth:accessoryTrailingView];
      if (maximumWidth < CGFLOAT_MAX) {
        [self.widthAnchor constraintLessThanOrEqualToConstant:maximumWidth]
            .active = YES;
      }
    }
  }

  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius = [self cornerRadius];
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    self.layer.shadowRadius = kShadowRadius;
    self.layer.shadowOffset = CGSizeMake(0, kShadowVerticalOffset);
    self.layer.shadowOpacity = kShadowOpacity;
    self.layer.shadowColor = [UIColor colorNamed:kGrey400Color].CGColor;
    self.layer.masksToBounds = NO;
  }
}

#pragma mark - UIResponder

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:[UIColor colorNamed:kGrey300Color]];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  // No op. This method implementation is needed per the UIResponder docs.
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:[self customBackgroundColor]];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:[self customBackgroundColor]];

  // Don't count touches ending outside the view as as taps.
  CGPoint locationInView = [touches.anyObject locationInView:self];
  if (CGRectContainsPoint(self.bounds, locationInView)) {
    [_delegate didTapFormSuggestionLabel:self];
  }
}

#pragma mark - Private

// Color of the suggestion chips.
- (UIColor*)customBackgroundColor {
  return
      [UIColor colorNamed:IsKeyboardAccessoryUpgradeEnabled() ? kSolidWhiteColor
                                                              : kGrey100Color];
}

// Corner radius of the suggestion chips.
- (CGFloat)cornerRadius {
  return IsKeyboardAccessoryUpgradeEnabled() ? kCornerRadius
                                             : self.bounds.size.height / 2.0;
}

// Computes the suggestion label's maximum width.
// Returns CGFLOAT_MAX if there's no maximum width.
- (CGFloat)maximumWidth:(UIView*)accessoryTrailingView {
  CGFloat maxWidth = CGFLOAT_MAX;
  // We're using the screen width because the 'window' member is nil at the
  // moment of setting up the label's width anchor.
  CGSize windowSize = [[UIScreen mainScreen] bounds].size;
  CGFloat portraitScreenWidth = MIN(windowSize.width, windowSize.height);
  switch (_suggestion.popupItemId) {
    case autofill::PopupItemId::kCreditCardEntry: {
      // Max width is just enough to show half of the credit card icon on the
      // 2nd suggestion, in portrait mode.
      CGFloat staticButtonsWidth = accessoryTrailingView.frame.size.width;
      maxWidth = (portraitScreenWidth - staticButtonsWidth) -
                 kHalfCreditCardIconOffset;
    } break;
    case autofill::PopupItemId::kAddressEntry:
      // Max width is half width, in portrait mode.
      maxWidth = portraitScreenWidth * 0.5;
      break;
    default:
      break;
  }
  return maxWidth;
}

@end
