// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_suggestion_label.h"

#import <QuartzCore/QuartzCore.h>
#include <stddef.h>

#include <cmath>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/credit_card.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/form_suggestion_view_client.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// a11y identifier used to locate the autofill suggestion in automation
NSString* const kFormSuggestionLabelAccessibilityIdentifier =
    @"formSuggestionLabelAXID";

namespace {

// The button corner radius.
const CGFloat kCornerRadius = 2.0f;

// Font size of button titles.
const CGFloat kIpadFontSize = 15.0f;
const CGFloat kIphoneFontSize = 14.0f;

// The alpha values of the suggestion's main and description labels.
const CGFloat kMainLabelAlpha = 0.87f;
const CGFloat kDescriptionLabelAlpha = 0.55f;

// The horizontal space between the edge of the background and the text.
const CGFloat kBorderWidth = 8.0f;
// The space between items in the label.
const CGFloat kSpacing = 4.0f;

// RGB button color when the button is not pressed.
const int kBackgroundNormalColor = 0xeceff1;
// RGB button color when the button is pressed.
const int kBackgroundPressedColor = 0xc4cbcf;

// Structure that record the image for each icon.
struct IconImageMap {
  const char* const icon_name;
  NSString* image_name;
};

// Creates a label with the given |text| and |alpha| suitable for use in a
// suggestion button in the keyboard accessory view.
UILabel* TextLabel(NSString* text, CGFloat alpha, BOOL bold) {
  UILabel* label = [[UILabel alloc] init];
  [label setText:text];
  CGFloat fontSize = IsIPadIdiom() ? kIpadFontSize : kIphoneFontSize;
  UIFont* font = bold ? [UIFont boldSystemFontOfSize:fontSize]
                      : [UIFont systemFontOfSize:fontSize];
  [label setFont:font];
  [label setTextColor:[UIColor colorWithWhite:0.0f alpha:alpha]];
  [label setBackgroundColor:[UIColor clearColor]];
  return label;
}

}  // namespace

@implementation FormSuggestionLabel {
  // Client of this view.
  __weak id<FormSuggestionViewClient> client_;
  FormSuggestion* suggestion_;
  BOOL userInteractionEnabled_;
}

- (id)initWithSuggestion:(FormSuggestion*)suggestion
                     index:(NSUInteger)index
    userInteractionEnabled:(BOOL)userInteractionEnabled
            numSuggestions:(NSUInteger)numSuggestions
                    client:(id<FormSuggestionViewClient>)client {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    suggestion_ = suggestion;
    client_ = client;
    userInteractionEnabled_ = userInteractionEnabled;

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

    UILabel* label = TextLabel(suggestion.value, kMainLabelAlpha, YES);
    [stackView addArrangedSubview:label];

    if ([suggestion.displayDescription length] > 0) {
      UILabel* description =
          TextLabel(suggestion.displayDescription, kDescriptionLabelAlpha, NO);
      [stackView addArrangedSubview:description];
    }

    if (userInteractionEnabled_) {
      [self setBackgroundColor:UIColorFromRGB(kBackgroundNormalColor)];
    }
    [[self layer] setCornerRadius:kCornerRadius];

    [self setClipsToBounds:YES];
    [self setUserInteractionEnabled:YES];
    [self setIsAccessibilityElement:YES];
    [self setAccessibilityLabel:l10n_util::GetNSStringF(
                                    IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION,
                                    base::SysNSStringToUTF16(suggestion.value),
                                    base::SysNSStringToUTF16(
                                        suggestion.displayDescription),
                                    base::IntToString16(index + 1),
                                    base::IntToString16(numSuggestions))];
    [self
        setAccessibilityIdentifier:kFormSuggestionLabelAccessibilityIdentifier];
  }

  return self;
}

#pragma mark -
#pragma mark UIResponder

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  if (userInteractionEnabled_) {
    [self setBackgroundColor:UIColorFromRGB(kBackgroundPressedColor)];
  }
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  if (userInteractionEnabled_) {
    [self setBackgroundColor:UIColorFromRGB(kBackgroundNormalColor)];
  }
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  if (userInteractionEnabled_) {
    [self setBackgroundColor:UIColorFromRGB(kBackgroundNormalColor)];
    [client_ didSelectSuggestion:suggestion_];
  }
}

@end
