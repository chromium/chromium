// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/button_util.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

// Alpha value for the disabled action button.
const CGFloat kDisabledButtonAlpha = 0.5;

// Updates `configuration` with the configuration shared with all buttons, and
// sets its `font`.
void CommonSetupButtonConfiguration(UIButtonConfiguration* configuration,
                                    UIFont* font) {
  configuration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  configuration.background.cornerRadius = kPrimaryButtonCornerRadius;

  configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [incoming mutableCopy];
    outgoing[NSFontAttributeName] = font;
    return outgoing;
  };
}

// Creates a button configured for all cases.
ChromeButton* CreateCommonButton() {
  ChromeButton* button = [ChromeButton buttonWithType:UIButtonTypeSystem];

  button.configuration = [UIButtonConfiguration plainButtonConfiguration];

  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

  return button;
}

// Returns a configuration update handler to be used for primary action.
UIButtonConfigurationUpdateHandler PrimaryActionConfigurationUpdateHandler() {
  return ^(UIButton* button) {
    UIColor* background_color = [UIColor colorNamed:kBlueColor];
    if (button.state & UIControlStateTunedDown) {
      background_color = [UIColor colorNamed:kBlue100Color];
    } else if (!button.enabled) {
      background_color = [UIColor colorNamed:kGrey100Color];
    }
    UIButtonConfiguration* configuration = button.configuration;
    configuration.background.backgroundColor = background_color;
    button.configuration = configuration;
  };
}

// Returns a configuration update handler to be used for primary action.
UIButtonConfigurationUpdateHandler
PrimaryDestructiveActionConfigurationUpdateHandler() {
  return ^(UIButton* button) {
    UIColor* background_color = [UIColor colorNamed:kRedColor];
    if (button.state & UIControlStateTunedDown) {
      background_color = [UIColor colorNamed:kRed100Color];
    } else if (!button.enabled) {
      background_color =
          [background_color colorWithAlphaComponent:kDisabledButtonAlpha];
    }
    UIButtonConfiguration* configuration = button.configuration;
    configuration.background.backgroundColor = background_color;
    button.configuration = configuration;
  };
}

// Returns a configuration update handler to be used for equal weight actions.
UIButtonConfigurationUpdateHandler EqualWeightConfigurationUpdateHandler() {
  return ^(UIButton* button) {
    UIButtonConfiguration* configuration = button.configuration;
    UIColor* background_color = [UIColor colorNamed:kBlueHaloColor];
    if (!button.enabled) {
      background_color =
          [background_color colorWithAlphaComponent:kDisabledButtonAlpha];
    }
    configuration.background.backgroundColor = background_color;
    button.configuration = configuration;
  };
}

// Updates `configuration` to match a Primary action.
void SetConfigurationForPrimaryAction(UIButtonConfiguration* configuration) {
  CommonSetupButtonConfiguration(
      configuration,
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]);
  configuration.baseForegroundColor =
      [UIColor colorNamed:kSolidButtonTextColor];
  configuration.background.backgroundColor = [UIColor colorNamed:kBlueColor];
}

// Updates `configuration` to match a Secondary action.
void SetConfigurationForSecondaryAction(UIButtonConfiguration* configuration) {
  CommonSetupButtonConfiguration(
      configuration, [UIFont preferredFontForTextStyle:UIFontTextStyleBody]);
  configuration.background.backgroundColor = [UIColor clearColor];
  configuration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
}

// Updates `configuration` to match an equal weight action.
void SetConfigurationForEqualWeight(UIButtonConfiguration* configuration) {
  CommonSetupButtonConfiguration(
      configuration,
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]);
  configuration.background.backgroundColor =
      [UIColor colorNamed:kBlueHaloColor];
  configuration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
}

}  // namespace

const UIControlState UIControlStateTunedDown = 1 << 16;

const CGFloat kButtonVerticalInsets = 14.5;
const CGFloat kPrimaryButtonCornerRadius = 15;

void UpdateButtonToMatchPrimaryAction(ChromeButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  SetConfigurationForPrimaryAction(configuration);
  button.configuration = configuration;
  button.configurationUpdateHandler = PrimaryActionConfigurationUpdateHandler();
}

void UpdateButtonToMatchPrimaryDestructiveAction(ChromeButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  CommonSetupButtonConfiguration(
      configuration,
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]);
  configuration.baseForegroundColor =
      [UIColor colorNamed:kSolidButtonTextColor];
  configuration.background.backgroundColor = [UIColor colorNamed:kRedColor];
  button.configuration = configuration;
  button.configurationUpdateHandler =
      PrimaryDestructiveActionConfigurationUpdateHandler();
}

void UpdateButtonToMatchSecondaryAction(ChromeButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  SetConfigurationForSecondaryAction(configuration);
  button.configuration = configuration;
  button.configurationUpdateHandler = nil;
}

void UpdateButtonToMatchEqualWeightAction(ChromeButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  SetConfigurationForEqualWeight(configuration);
  button.configuration = configuration;
  button.configurationUpdateHandler = EqualWeightConfigurationUpdateHandler();
}

ChromeButton* PrimaryActionButton() {
  ChromeButton* button = CreateCommonButton();
  UpdateButtonToMatchPrimaryAction(button);
  return button;
}

ChromeButton* PrimaryDestructiveActionButton() {
  ChromeButton* button = CreateCommonButton();
  UpdateButtonToMatchPrimaryDestructiveAction(button);
  return button;
}

ChromeButton* SecondaryActionButton() {
  ChromeButton* button = CreateCommonButton();
  UpdateButtonToMatchSecondaryAction(button);
  return button;
}

// Returns equal weight button with rounded corners.
ChromeButton* EqualWeightButton() {
  ChromeButton* button = CreateCommonButton();
  UpdateButtonToMatchEqualWeightAction(button);
  return button;
}

void SetConfigurationTitle(UIButton* button, NSString* newString) {
  UIButtonConfiguration* buttonConfiguration = button.configuration;
  buttonConfiguration.title = newString;
  button.configuration = buttonConfiguration;
}

void SetConfigurationFont(UIButton* button, UIFont* font) {
  UIButtonConfiguration* buttonConfiguration = button.configuration;

  buttonConfiguration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [incoming mutableCopy];
    outgoing[NSFontAttributeName] = font;
    return outgoing;
  };

  button.configuration = buttonConfiguration;
}
