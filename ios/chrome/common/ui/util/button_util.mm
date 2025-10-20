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
// sets its `font`. The color is set based on the button state
// (enabled/disabled). If the 'button' is nil, only the enabled color will be
// used.
void CommonSetupButtonConfiguration(UIButtonConfiguration* configuration,
                                    UIFont* font,
                                    UIColor* enabled_color,
                                    UIButton* button = nil,
                                    UIColor* disabled_color = nil) {
  configuration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);

  if (@available(iOS 26, *)) {
    configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  } else {
    configuration.background.cornerRadius = kPrimaryButtonCornerRadius;
  }

  __weak UIButton* weak_button = button;

  configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    BOOL use_enabled_color = !weak_button || weak_button.enabled;
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [incoming mutableCopy];
    outgoing[NSFontAttributeName] = font;
    outgoing[NSForegroundColorAttributeName] =
        use_enabled_color ? enabled_color : disabled_color;
    return outgoing;
  };
}

// Creates a button configured for all cases.
ChromeButton* CreateCommonButton() {
  ChromeButton* button = [ChromeButton buttonWithType:UIButtonTypeSystem];
  if (@available(iOS 26, *)) {
    if ([UIButtonConfiguration
            respondsToSelector:@selector(prominentGlassButtonConfiguration)]) {
      button.configuration =
          [UIButtonConfiguration prominentGlassButtonConfiguration];
    } else {
      button.configuration = [UIButtonConfiguration glassButtonConfiguration];
    }
  } else {
    button.configuration = [UIButtonConfiguration plainButtonConfiguration];
  }

  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

  return button;
}

// Returns a configuration update handler to be used for primary action,
// `destructive` or not.
UIButtonConfigurationUpdateHandler PrimaryActionConfigurationUpdateHandler(
    bool destructive) {
  return ^(UIButton* button) {
    bool background_as_tint;
    if (@available(iOS 26, *)) {
      // On iOS 26 when the button is disabled, the tint is not used to color
      // the glass. In that case, set the background color directly.
      background_as_tint = button.enabled;
    } else {
      background_as_tint = false;
    }

    UIColor* background_color = destructive ? [UIColor colorNamed:kRedColor]
                                            : [UIColor colorNamed:kBlueColor];
    CGFloat alpha = 1;
    if (button.state & UIControlStateTunedDown) {
      background_color = destructive ? [UIColor colorNamed:kRed100Color]
                                     : [UIColor colorNamed:kBlue100Color];
    } else if (!button.enabled) {
      background_color = [UIColor colorNamed:kGrey400Color];
      alpha = kDisabledButtonAlpha;
    }

    UIButtonConfiguration* configuration = button.configuration;
    if (background_as_tint) {
      configuration.background.backgroundColor = UIColor.clearColor;
      button.tintColor = background_color;
    } else {
      configuration.background.backgroundColor = background_color;
      button.tintColor = UIColor.clearColor;
    }
    button.alpha = alpha;
    button.configuration = configuration;
  };
}

// Returns a configuration update handler to be used for non-primary action.
UIButtonConfigurationUpdateHandler
NonPrimaryActionConfigurationUpdateHandler() {
  return ^(UIButton* button) {
    button.alpha = button.enabled ? 1 : kDisabledButtonAlpha;
  };
}

}  // namespace

const UIControlState UIControlStateTunedDown = 1 << 16;

const CGFloat kButtonVerticalInsets = 14.5;
const CGFloat kPrimaryButtonCornerRadius = 15;

void UpdateButtonToMatchPrimaryAction(ChromeButton* button) {
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  UIColor* enabled_text_color = [UIColor colorNamed:kSolidButtonTextColor];
  UIColor* disabled_text_color = [UIColor colorNamed:kSolidBlackColor];
  UIButtonConfiguration* configuration = button.configuration;
  CommonSetupButtonConfiguration(configuration, font, enabled_text_color,
                                 button, disabled_text_color);
  configuration.baseForegroundColor = enabled_text_color;
  if (@available(iOS 26, *)) {
    configuration.background.backgroundColor = UIColor.clearColor;
    button.tintColor = [UIColor colorNamed:kBlueColor];
  } else {
    configuration.background.backgroundColor = [UIColor colorNamed:kBlueColor];
  }
  button.configuration = configuration;
  button.configurationUpdateHandler =
      PrimaryActionConfigurationUpdateHandler(/*destructive=*/false);
}

void UpdateButtonToMatchPrimaryDestructiveAction(ChromeButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  UIColor* enabled_text_color = [UIColor colorNamed:kSolidButtonTextColor];
  UIColor* disabled_text_color = [UIColor colorNamed:kSolidBlackColor];
  CommonSetupButtonConfiguration(
      configuration, [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline],
      enabled_text_color, button, disabled_text_color);
  configuration.baseForegroundColor = enabled_text_color;
  if (@available(iOS 26, *)) {
    configuration.background.backgroundColor = UIColor.clearColor;
    button.tintColor = [UIColor colorNamed:kRedColor];
  } else {
    configuration.background.backgroundColor = [UIColor colorNamed:kRedColor];
  }
  button.configuration = configuration;
  button.configurationUpdateHandler =
      PrimaryActionConfigurationUpdateHandler(/*destructive=*/true);
}

void UpdateButtonToMatchSecondaryAction(ChromeButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  UIColor* enabled_text_color;
  UIFont* font;
  if (@available(iOS 26, *)) {
    enabled_text_color = [UIColor colorNamed:kSolidBlackColor];
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    button.tintColor = UIColor.clearColor;
  } else {
    enabled_text_color = [UIColor colorNamed:kBlueColor];
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    configuration.background.backgroundColor = UIColor.clearColor;
  }
  CommonSetupButtonConfiguration(configuration, font, enabled_text_color);
  configuration.baseForegroundColor = enabled_text_color;
  button.configuration = configuration;
  button.configurationUpdateHandler =
      NonPrimaryActionConfigurationUpdateHandler();
}

void UpdateButtonToMatchTertiaryAction(ChromeButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  UIColor* enabled_text_color = [UIColor colorNamed:kBlueColor];
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  CommonSetupButtonConfiguration(configuration, font, enabled_text_color);
  configuration.baseForegroundColor = enabled_text_color;
  if (@available(iOS 26, *)) {
    configuration.background.backgroundColor = UIColor.clearColor;
    button.tintColor = [UIColor colorNamed:kBlueHaloColor];
  } else {
    configuration.background.backgroundColor =
        [UIColor colorNamed:kBlueHaloColor];
  }
  button.configuration = configuration;
  button.configurationUpdateHandler =
      NonPrimaryActionConfigurationUpdateHandler();
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

ChromeButton* TertiaryActionButton() {
  ChromeButton* button = CreateCommonButton();
  UpdateButtonToMatchTertiaryAction(button);
  return button;
}

void SetConfigurationTitle(UIButton* button, NSString* newString) {
  UIButtonConfiguration* button_configuration = button.configuration;
  button_configuration.title = newString;
  button.configuration = button_configuration;
}

void SetConfigurationFont(UIButton* button, UIFont* font) {
  UIButtonConfiguration* button_configuration = button.configuration;

  button_configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [incoming mutableCopy];
    outgoing[NSFontAttributeName] = font;
    return outgoing;
  };

  button.configuration = button_configuration;
}

void SetConfigurationImage(ChromeButton* button, UIImage* image) {
  UIButtonConfiguration* button_configuration = button.configuration;
  button_configuration.image = image;
  button.configuration = button_configuration;
}
