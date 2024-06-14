// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/button_util.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

const CGFloat kButtonVerticalInsets = 14.5;
const CGFloat kPrimaryButtonCornerRadius = 15;
// Alpha value for the disabled action button.
const CGFloat kDisabledButtonAlpha = 0.5;

UIButton* PrimaryActionButton(BOOL pointer_interaction_enabled) {
  UIButton* primary_blue_button = [UIButton buttonWithType:UIButtonTypeSystem];
  primary_blue_button.translatesAutoresizingMaskIntoConstraints = NO;

  if (@available(iOS 15.0, *)) {
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
    buttonConfiguration.background.backgroundColor =
        [UIColor colorNamed:kBlueColor];
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kSolidButtonTextColor];
    buttonConfiguration.background.cornerRadius = kPrimaryButtonCornerRadius;

    UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSMutableAttributedString* string =
        [[NSMutableAttributedString alloc] initWithString:@" "];
    [string addAttributes:attributes range:NSMakeRange(0, string.length)];
    buttonConfiguration.attributedTitle = string;

    primary_blue_button.configuration = buttonConfiguration;
  }

  if (pointer_interaction_enabled) {
    primary_blue_button.pointerInteractionEnabled = YES;
    primary_blue_button.pointerStyleProvider =
        CreateOpaqueButtonPointerStyleProvider();
  }

  return primary_blue_button;
}

void SetConfigurationTitle(UIButton* button, NSString* newString) {
  if (@available(iOS 15.0, *)) {
    UIButtonConfiguration* buttonConfiguration = button.configuration;
    NSMutableAttributedString* attributedString =
        [[NSMutableAttributedString alloc]
            initWithAttributedString:buttonConfiguration.attributedTitle];
    [attributedString.mutableString setString:newString ? newString : @""];
    buttonConfiguration.attributedTitle = attributedString;
    button.configuration = buttonConfiguration;
  }
}

void SetConfigurationFont(UIButton* button, UIFont* font) {
  if (@available(iOS 15.0, *)) {
    UIButtonConfiguration* buttonConfiguration = button.configuration;
    NSString* configurationString = buttonConfiguration.attributedTitle.string;

    if (configurationString) {
      NSDictionary* attributes = @{NSFontAttributeName : font};
      NSMutableAttributedString* string = [[NSMutableAttributedString alloc]
          initWithString:configurationString];
      [string addAttributes:attributes range:NSMakeRange(0, string.length)];
      buttonConfiguration.attributedTitle = string;
      button.configuration = buttonConfiguration;
    }
  }
}

void UpdateButtonColorOnEnableDisable(UIButton* button) {
  if (@available(iOS 15.0, *)) {
    UIButtonConfiguration* buttonConfiguration = button.configuration;
    if (button.enabled) {
      buttonConfiguration.background.backgroundColor =
          [UIColor colorNamed:kBlueColor];
      buttonConfiguration.baseForegroundColor =
          [UIColor colorNamed:kSolidButtonTextColor];
    } else {
      buttonConfiguration.background.backgroundColor =
          [buttonConfiguration.background.backgroundColor
              colorWithAlphaComponent:kDisabledButtonAlpha];
      buttonConfiguration.baseForegroundColor =
          [buttonConfiguration.baseForegroundColor
              colorWithAlphaComponent:kDisabledButtonAlpha];
    }
    button.configuration = buttonConfiguration;
  }
}
