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
#include "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/form_suggestion_client.h"
#import "ios/chrome/browser/autofill/form_suggestion_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

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

// Structure that record the image for each icon.
struct IconImageMap {
  const char* const icon_name;
  NSString* image_name;
};

// Creates a label with the given |text| and |alpha| suitable for use in a
// suggestion button in the keyboard accessory view.
UILabel* TextLabel(NSString* text, UIColor* textColor, BOOL bold) {
  UILabel* label = [[UILabel alloc] init];
  [label setText:text];
  CGFloat fontSize = IsIPadIdiom() ? kIpadFontSize : kIphoneFontSize;
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
  __weak id<FormSuggestionClient> client_;
  FormSuggestion* suggestion_;
}

- (id)initWithSuggestion:(FormSuggestion*)suggestion
                     index:(NSUInteger)index
            numSuggestions:(NSUInteger)numSuggestions
                    client:(id<FormSuggestionClient>)client {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    suggestion_ = suggestion;
    client_ = client;

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
    [stackView addArrangedSubview:label];

    if ([suggestion.displayDescription length] > 0) {
      UILabel* description =
          TextLabel(suggestion.displayDescription,
                    [UIColor colorNamed:kTextSecondaryColor], NO);
      [stackView addArrangedSubview:description];
    }

    [self setBackgroundColor:[UIColor colorNamed:kGrey100Color]];

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

#pragma mark -
#pragma mark UIResponder

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:[UIColor colorNamed:kGrey300Color]];
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:[UIColor colorNamed:kGrey100Color]];
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:[UIColor colorNamed:kGrey100Color]];
  [client_ didSelectSuggestion:suggestion_];
}

@end
