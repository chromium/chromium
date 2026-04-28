// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/chrome_button.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

const UIControlState UIControlStateTunedDown = 1 << 16;

// Alpha value for the disabled action button.
const CGFloat kDisabledButtonAlpha = 0.5;

// The padding between the custom image and the title.
const CGFloat kCustomImagePadding = 4.0;

// The point size for the checkmark image.
const CGFloat kCheckmarkImagePointSize = 17.0;

// Returns whether `button` should have its background tinted or not.
bool ShouldUseTintColor(UIButton* button) {
  if (@available(iOS 26, *)) {
    if (@available(iOS 26.1, *)) {
      // On iOS 26.1 there is a bug on the text color of the buttons when
      // tinting them. See b/458680641.
      return false;
    }
    // On iOS 26 when the button is disabled, the tint is not used to color
    // the glass. In that case, set the background color directly.
    return button.enabled;
  }
  return false;
}

// Returns the color to be used by a primary `destructive` `button`.
UIColor* PrimaryButtonBackgroundColor(UIButton* button, bool destructive) {
  UIColor* background_color = destructive ? [UIColor colorNamed:kRedColor]
                                          : [UIColor colorNamed:kBlueColor];
  if (button.state & UIControlStateTunedDown) {
    background_color = destructive ? [UIColor colorNamed:kRed100Color]
                                   : [UIColor colorNamed:kBlue100Color];
  } else if (!button.enabled) {
    background_color = [UIColor colorNamed:kGrey400Color];
  }

  return background_color;
}

// Updates the background color of a primary `button` with a `destructive`
// style, using the `configuration`.
void UpdatePrimaryButtonBackgroundColor(UIButton* button,
                                        bool destructive,
                                        UIButtonConfiguration* configuration) {
  bool background_as_tint = ShouldUseTintColor(button);

  UIColor* background_color = PrimaryButtonBackgroundColor(button, destructive);
  CGFloat alpha = (!(button.state & UIControlStateTunedDown) && !button.enabled)
                      ? kDisabledButtonAlpha
                      : 1;

  if (background_as_tint) {
    configuration.background.backgroundColor = UIColor.clearColor;
    button.tintColor = background_color;
  } else {
    configuration.background.backgroundColor = background_color;
    button.tintColor = UIColor.clearColor;
  }
  button.alpha = alpha;
}

// Updates the background color of a primary `button` with a `destructive`
// style.
void UpdatePrimaryButtonBackgroundColor(UIButton* button, bool destructive) {
  UIButtonConfiguration* configuration = button.configuration;
  UpdatePrimaryButtonBackgroundColor(button, destructive, configuration);
  button.configuration = configuration;
}

// Updates `configuration` with the given `font` and `color`.
void SetButtonTitleTextAttributes(UIButtonConfiguration* configuration,
                                  UIFont* font,
                                  UIColor* enabled_color,
                                  UIButton* button = nil,
                                  UIColor* disabled_color = nil) {
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

// Returns a configuration update handler to be used for primary action,
// `destructive` or not.
UIButtonConfigurationUpdateHandler PrimaryActionConfigurationUpdateHandler(
    bool destructive) {
  return ^(UIButton* button) {
    UpdatePrimaryButtonBackgroundColor(button, destructive);
  };
}

// Returns a configuration update handler to be used for non-primary action.
UIButtonConfigurationUpdateHandler
NonPrimaryActionConfigurationUpdateHandler() {
  return ^(UIButton* button) {
    button.alpha = button.enabled ? 1 : kDisabledButtonAlpha;
  };
}

// Returns the checkmark image with the correct configuration.
UIImage* CheckmarkImage() {
  UIImageSymbolConfiguration* symbol_configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCheckmarkImagePointSize];
  return [UIImage systemImageNamed:@"checkmark.circle.fill"
                 withConfiguration:symbol_configuration];
}

// Configures the image color for the given button configuration.
void ConfigureImageColor(UIButtonConfiguration* button_configuration,
                         UIColor* color) {
  if (@available(iOS 26, *)) {
    button_configuration.image = [button_configuration.image
        imageWithTintColor:color
             renderingMode:UIImageRenderingModeAlwaysOriginal];
  } else {
    button_configuration.imageColorTransformer = ^UIColor*(UIColor* _) {
      return color;
    };
  }
}
}  // namespace

@implementation ChromeButton {
  // Whether the initial button configuration is done.
  BOOL _initialConfigurationDone;
}

- (instancetype)initWithStyle:(ChromeButtonStyle)style {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self setupInitialConfiguration];
    self.style = style;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.pointerInteractionEnabled = YES;
    self.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();
    _initialConfigurationDone = YES;
  }
  return self;
}

#pragma mark - Properties

- (void)setStyle:(ChromeButtonStyle)style {
  if (_initialConfigurationDone && _style == style) {
    return;
  }
  _style = style;
  switch (_style) {
    case ChromeButtonStylePrimary:
      [self updateButtonToMatchPrimaryAction];
      break;
    case ChromeButtonStylePrimaryDestructive:
      [self updateButtonToMatchPrimaryDestructiveAction];
      break;
    case ChromeButtonStyleSecondary:
      [self updateButtonToMatchSecondaryAction];
      break;
    case ChromeButtonStyleSecondaryDestructive:
      [self updateButtonToMatchSecondaryDestructiveAction];
      break;
    case ChromeButtonStyleTertiary:
      [self updateButtonToMatchTertiaryAction];
      break;
  }
}

- (void)setTunedDownStyle:(BOOL)tunedDownStyle {
  if (_tunedDownStyle == tunedDownStyle) {
    return;
  }
  _tunedDownStyle = tunedDownStyle;
  [self setNeedsUpdateConfiguration];
}

- (void)setTitle:(NSString*)title {
  UIButtonConfiguration* buttonConfiguration = self.configuration;
  buttonConfiguration.title = title;
  self.configuration = buttonConfiguration;
}

- (NSString*)title {
  return self.configuration.title;
}

- (void)setFont:(UIFont*)font {
  UIFont* currentFont = self.font;
  if ([currentFont isEqual:font]) {
    return;
  }

  UIButtonConfiguration* configuration = self.configuration;
  UIConfigurationTextAttributesTransformer originalTransformer =
      configuration.titleTextAttributesTransformer;

  configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSDictionary<NSAttributedStringKey, id>* transformed = incoming;
    if (originalTransformer) {
      transformed = originalTransformer(incoming);
    }
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [transformed mutableCopy];
    if (font) {
      outgoing[NSFontAttributeName] = font;
    } else {
      [outgoing removeObjectForKey:NSFontAttributeName];
    }
    return outgoing;
  };

  self.configuration = configuration;
}

- (UIFont*)font {
  if (!self.configuration.titleTextAttributesTransformer) {
    return nil;
  }
  NSDictionary<NSAttributedStringKey, id>* attributes =
      self.configuration.titleTextAttributesTransformer(@{});
  return attributes[NSFontAttributeName];
}

- (void)setPrimaryButtonImage:(PrimaryButtonImage)primaryButtonImage {
  _primaryButtonImage = primaryButtonImage;
  UIButtonConfiguration* buttonConfiguration = self.configuration;

  if (primaryButtonImage != PrimaryButtonImageCustom) {
    buttonConfiguration.image = nil;
  }
  buttonConfiguration.imageColorTransformer = nil;
  buttonConfiguration.showsActivityIndicator = NO;
  buttonConfiguration.imagePadding = kCustomImagePadding;
  UIColor* color = [UIColor colorNamed:kSolidButtonTextColor];

  switch (primaryButtonImage) {
    case PrimaryButtonImageNone:
      break;
    case PrimaryButtonImageSpinner: {
      buttonConfiguration.showsActivityIndicator = YES;
      buttonConfiguration.activityIndicatorColorTransformer =
          ^UIColor*(UIColor* _) {
            return color;
          };
      break;
    }
    case PrimaryButtonImageCheckmark: {
      buttonConfiguration.image = CheckmarkImage();
      if (self.state & UIControlStateTunedDown) {
        if (self.style == ChromeButtonStylePrimary) {
          color = [UIColor colorNamed:kBlue700Color];
        } else if (self.style == ChromeButtonStylePrimaryDestructive) {
          color = [UIColor colorNamed:kRed600Color];
        }
      }
      ConfigureImageColor(buttonConfiguration, color);
      break;
    }
    case PrimaryButtonImageCustom: {
      ConfigureImageColor(buttonConfiguration, color);
      break;
    }
  }

  self.configuration = buttonConfiguration;
}

- (UIControlState)state {
  UIControlState originalState = [super state];
  if (self.tunedDownStyle) {
    return originalState | UIControlStateTunedDown;
  }
  return originalState;
}

#pragma mark - Private

// Sets up the initial configuration for the button.
- (void)setupInitialConfiguration {
  if (@available(iOS 26, *)) {
    if (@available(iOS 26.1, *)) {
      // On iOS 26.1, there is an issue with the text color, which is not
      // respecting its color, making it very low contrast. Use a glass button
      // with a background color instead of prominent with tint. See
      // b/458680641.
      self.configuration = [UIButtonConfiguration glassButtonConfiguration];
    } else {
      self.configuration =
          [UIButtonConfiguration prominentGlassButtonConfiguration];
    }
  } else {
    self.configuration = [UIButtonConfiguration plainButtonConfiguration];
  }

  UIButtonConfiguration* configuration = self.configuration;
  configuration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  if (@available(iOS 26, *)) {
    configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  } else {
    configuration.background.cornerRadius = kPrimaryButtonCornerRadius;
  }
  self.configuration = configuration;
}

// Updates `button` to match a primary action style.
- (void)updateButtonToMatchPrimaryAction {
  UIButtonConfiguration* configuration = self.configuration;
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  UIColor* enabledTextColor = [UIColor colorNamed:kSolidButtonTextColor];
  UIColor* disabledTextColor = [UIColor colorNamed:kSolidBlackColor];
  SetButtonTitleTextAttributes(configuration, font, enabledTextColor, self,
                               disabledTextColor);
  configuration.baseForegroundColor = enabledTextColor;
  UpdatePrimaryButtonBackgroundColor(self, /*destructive*/ false,
                                     configuration);
  self.configuration = configuration;
  self.configurationUpdateHandler =
      PrimaryActionConfigurationUpdateHandler(/*destructive=*/false);
}

// Updates `button` to match a primary destruction action style.
- (void)updateButtonToMatchPrimaryDestructiveAction {
  UIButtonConfiguration* configuration = self.configuration;
  UIColor* enabledTextColor = [UIColor colorNamed:kSolidButtonTextColor];
  UIColor* disabledTextColor = [UIColor colorNamed:kSolidBlackColor];
  SetButtonTitleTextAttributes(
      configuration, [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline],
      enabledTextColor, self, disabledTextColor);
  configuration.baseForegroundColor = enabledTextColor;
  UpdatePrimaryButtonBackgroundColor(self, /*destructive*/ true, configuration);
  self.configuration = configuration;
  self.configurationUpdateHandler =
      PrimaryActionConfigurationUpdateHandler(/*destructive=*/true);
}

// Updates `button` to match a secondary action style.
- (void)updateButtonToMatchSecondaryAction {
  UIButtonConfiguration* configuration = self.configuration;
  UIColor* enabledTextColor;
  UIFont* font;
  if (@available(iOS 26, *)) {
    enabledTextColor = [UIColor colorNamed:kSolidBlackColor];
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    self.tintColor = UIColor.clearColor;
    configuration.background.backgroundColor = UIColor.clearColor;
  } else {
    enabledTextColor = [UIColor colorNamed:kBlueColor];
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    configuration.background.backgroundColor = UIColor.clearColor;
  }
  SetButtonTitleTextAttributes(configuration, font, enabledTextColor);
  configuration.baseForegroundColor = enabledTextColor;
  self.configuration = configuration;
  self.configurationUpdateHandler =
      NonPrimaryActionConfigurationUpdateHandler();
}

// Updates `button` to match a secondary destructive action style.
- (void)updateButtonToMatchSecondaryDestructiveAction {
  UIButtonConfiguration* configuration = self.configuration;
  configuration.background.backgroundColor = UIColor.clearColor;
  UIColor* enabledTextColor = [UIColor colorNamed:kRed600Color];
  UIFont* font;
  if (@available(iOS 26, *)) {
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    self.tintColor = UIColor.clearColor;
  } else {
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  }
  SetButtonTitleTextAttributes(configuration, font, enabledTextColor);
  configuration.baseForegroundColor = enabledTextColor;
  self.configuration = configuration;
  self.configurationUpdateHandler =
      NonPrimaryActionConfigurationUpdateHandler();
}

// Updates `button` to match a tertiary action style.
- (void)updateButtonToMatchTertiaryAction {
  UIButtonConfiguration* configuration = self.configuration;
  UIColor* enabledTextColor = [UIColor colorNamed:kBlueColor];
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  SetButtonTitleTextAttributes(configuration, font, enabledTextColor);
  configuration.baseForegroundColor = enabledTextColor;
  if (@available(iOS 26, *)) {
    if (@available(iOS 26.1, *)) {
      configuration.background.backgroundColor =
          [UIColor colorNamed:kBlueHaloColor];
    } else {
      configuration.background.backgroundColor = UIColor.clearColor;
      self.tintColor = [UIColor colorNamed:kBlueHaloColor];
    }
  } else {
    configuration.background.backgroundColor =
        [UIColor colorNamed:kBlueHaloColor];
  }
  self.configuration = configuration;
  self.configurationUpdateHandler =
      NonPrimaryActionConfigurationUpdateHandler();
}

@end
