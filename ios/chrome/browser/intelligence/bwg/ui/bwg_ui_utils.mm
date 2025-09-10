// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_ui_utils.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"

@implementation BWGUIUtils

+ (UIButton*)createPrimaryButtonWithTitle:(NSString*)title {
  ChromeButton* primaryButton = PrimaryActionButton();
  UIButtonConfiguration* buttonConfiguration = primaryButton.configuration;

  UIFont* font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);
  NSDictionary<NSAttributedStringKey, id>* attributes =
      @{NSFontAttributeName : font};
  NSAttributedString* attributedTitle =
      [[NSAttributedString alloc] initWithString:title attributes:attributes];
  buttonConfiguration.attributedTitle = attributedTitle;
  primaryButton.configuration = buttonConfiguration;

  return primaryButton;
}

+ (UIButton*)createSecondaryButtonWithTitle:(NSString*)title {
  ChromeButton* secondaryButton = SecondaryActionButton();
  UIButtonConfiguration* buttonConfiguration = secondaryButton.configuration;
  buttonConfiguration.title = title;
  buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
  UIBackgroundConfiguration* backgroundConfig = buttonConfiguration.background;
  backgroundConfig.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  buttonConfiguration.background = backgroundConfig;

  secondaryButton.configuration = buttonConfiguration;

  return secondaryButton;
}

@end
