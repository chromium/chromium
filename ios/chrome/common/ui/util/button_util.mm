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
// sets its `font` and `color`.
void CommonSetupButtonConfiguration(UIButtonConfiguration* configuration,
                                    UIFont* font,
                                    UIColor* foregroundColor) {
  configuration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);

#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if (@available(iOS 26, *)) {
    configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  } else {
#endif
    configuration.background.cornerRadius = kPrimaryButtonCornerRadius;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  }
#endif

  configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [incoming mutableCopy];
    outgoing[NSFontAttributeName] = font;
    outgoing[NSForegroundColorAttributeName] = foregroundColor;
    return outgoing;
  };
}

// Creates a button configured for all cases.
ChromeButton* CreateCommonButton() {
  ChromeButton* button = [ChromeButton buttonWithType:UIButtonTypeSystem];
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if (@available(iOS 26, *)) {
    if ([UIButtonConfiguration
            respondsToSelector:@selector(prominentGlassButtonConfiguration)]) {
      button.configuration =
          [UIButtonConfiguration prominentGlassButtonConfiguration];
    } else {
      button.configuration = [UIButtonConfiguration glassButtonConfiguration];
    }
  } else {
#endif
    button.configuration = [UIButtonConfiguration plainButtonConfiguration];
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  }
#endif

  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

  return button;
}

// Returns a configuration update handler to be used for primary action.
UIButtonConfigurationUpdateHandler PrimaryActionConfigurationUpdateHandler() {
  return ^(UIButton* button) {
    bool background_as_tint;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
    if (@available(iOS 26, *)) {
      background_as_tint = true;
    } else {
#endif
      background_as_tint = false;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
    }
#endif

    UIColor* background_color = [UIColor colorNamed:kBlueColor];
    CGFloat alpha = 1;
    if (button.state & UIControlStateTunedDown) {
      background_color = [UIColor colorNamed:kBlue100Color];
    } else if (!button.enabled) {
      alpha = kDisabledButtonAlpha;
    }

    UIButtonConfiguration* configuration = button.configuration;
    if (background_as_tint) {
      configuration.background.backgroundColor = UIColor.clearColor;
      button.tintColor = background_color;
    } else {
      configuration.background.backgroundColor = background_color;
    }
    button.alpha = alpha;
    button.configuration = configuration;
  };
}

// Returns a configuration update handler to be used for destructive primary
// action.
UIButtonConfigurationUpdateHandler
PrimaryDestructiveActionConfigurationUpdateHandler() {
  return ^(UIButton* button) {
    bool background_as_tint;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
    if (@available(iOS 26, *)) {
      background_as_tint = true;
    } else {
#endif
      background_as_tint = false;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
    }
#endif

    UIColor* background_color = [UIColor colorNamed:kRedColor];
    CGFloat alpha = 1;
    if (button.state & UIControlStateTunedDown) {
      background_color = [UIColor colorNamed:kRed100Color];
    } else if (!button.enabled) {
      alpha = kDisabledButtonAlpha;
    }

    UIButtonConfiguration* configuration = button.configuration;
    if (background_as_tint) {
      configuration.background.backgroundColor = UIColor.clearColor;
      button.tintColor = background_color;
    } else {
      configuration.background.backgroundColor = background_color;
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
  UIColor* foregroundColor = [UIColor colorNamed:kSolidButtonTextColor];
  UIButtonConfiguration* configuration = button.configuration;
  CommonSetupButtonConfiguration(configuration, font, foregroundColor);
  configuration.baseForegroundColor = foregroundColor;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if (@available(iOS 26, *)) {
    configuration.background.backgroundColor = UIColor.clearColor;
    button.tintColor = [UIColor colorNamed:kBlueColor];
  } else {
#endif
    configuration.background.backgroundColor = [UIColor colorNamed:kBlueColor];
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  }
#endif
  button.configuration = configuration;
  button.configurationUpdateHandler = PrimaryActionConfigurationUpdateHandler();
}

void UpdateButtonToMatchPrimaryDestructiveAction(ChromeButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  UIColor* foregroundColor = [UIColor colorNamed:kSolidButtonTextColor];
  CommonSetupButtonConfiguration(
      configuration, [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline],
      foregroundColor);
  configuration.baseForegroundColor = foregroundColor;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if (@available(iOS 26, *)) {
    configuration.background.backgroundColor = UIColor.clearColor;
    button.tintColor = [UIColor colorNamed:kRedColor];
  } else {
#endif
    configuration.background.backgroundColor = [UIColor colorNamed:kRedColor];
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  }
#endif
  button.configuration = configuration;
  button.configurationUpdateHandler =
      PrimaryDestructiveActionConfigurationUpdateHandler();
}

void UpdateButtonToMatchSecondaryAction(ChromeButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  UIColor* foregroundColor;
  UIFont* font;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if (@available(iOS 26, *)) {
    foregroundColor = [UIColor colorNamed:kSolidBlackColor];
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    button.tintColor = UIColor.clearColor;
  } else {
#endif
    foregroundColor = [UIColor colorNamed:kBlueColor];
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    configuration.background.backgroundColor = UIColor.clearColor;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  }
#endif
  CommonSetupButtonConfiguration(configuration, font, foregroundColor);
  configuration.baseForegroundColor = foregroundColor;
  button.configuration = configuration;
  button.configurationUpdateHandler =
      NonPrimaryActionConfigurationUpdateHandler();
}

void UpdateButtonToMatchTertiaryAction(ChromeButton* button) {
  UIButtonConfiguration* configuration = button.configuration;
  UIColor* foregroundColor = [UIColor colorNamed:kBlueColor];
  CommonSetupButtonConfiguration(
      configuration, [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline],
      foregroundColor);
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if (@available(iOS 26, *)) {
    configuration.background.backgroundColor = UIColor.clearColor;
    button.tintColor = [UIColor colorNamed:kBlueHaloColor];
  } else {
#endif
    configuration.background.backgroundColor =
        [UIColor colorNamed:kBlueHaloColor];
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  }
#endif
  configuration.baseForegroundColor = foregroundColor;
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
