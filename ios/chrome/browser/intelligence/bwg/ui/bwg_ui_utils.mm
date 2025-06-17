// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_ui_utils.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

const CGFloat kPrimarySecondaryButtonHeight = 50.0;
const CGFloat kPrimarySecondaryButtonCornerRadius = 15.0;

@interface BWGUIUtils ()

// Configures common button properties.
+ (UIButton*)configureCommonButton;

@end

@implementation BWGUIUtils

+ (UIButton*)configureCommonButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.layer.masksToBounds = YES;
  [NSLayoutConstraint activateConstraints:@[
    [button.heightAnchor
        constraintEqualToConstant:kPrimarySecondaryButtonHeight]
  ]];

  return button;
}

+ (UIButton*)createPrimaryButtonWithTitle:(NSString*)title {
  UIButton* primaryButton = [self configureCommonButton];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfiguration.baseForegroundColor = [UIColor whiteColor];
  buttonConfiguration.title = title;

  UIBackgroundConfiguration* backgroundConfig =
      [UIBackgroundConfiguration clearConfiguration];
  backgroundConfig.backgroundColor = [UIColor colorNamed:kBlueColor];
  buttonConfiguration.background = backgroundConfig;

  primaryButton.configuration = buttonConfiguration;
  primaryButton.layer.cornerRadius = kPrimarySecondaryButtonCornerRadius;
  primaryButton.titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightBold);

  return primaryButton;
}

+ (UIButton*)createSecondaryButtonWithTitle:(NSString*)title {
  UIButton* secondaryButton = [self configureCommonButton];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
  buttonConfiguration.title = title;

  UIBackgroundConfiguration* backgroundConfig =
      [UIBackgroundConfiguration clearConfiguration];
  backgroundConfig.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  buttonConfiguration.background = backgroundConfig;
  secondaryButton.configuration = buttonConfiguration;

  secondaryButton.titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleCallout, UIFontWeightRegular);

  return secondaryButton;
}

@end
