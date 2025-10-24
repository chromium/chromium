// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/chrome_button.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

const UIControlState UIControlStateTunedDown = 1 << 16;

const CGFloat kButtonVerticalInsets = 14.5;
const CGFloat kPrimaryButtonCornerRadius = 15;

// Alpha value for the disabled action button.
const CGFloat kDisabledButtonAlpha = 0.5;

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

// Returns the checkmark image with the correct configuration.
UIImage* CheckmarkImage() {
  UIImageSymbolConfiguration* symbol_configuration =
      [UIImageSymbolConfiguration configurationWithPointSize:17];
  return [UIImage systemImageNamed:@"checkmark.circle.fill"
                 withConfiguration:symbol_configuration];
}

}  // namespace

@implementation ChromeButton {
  // Wether the inital button configuration is done.
  BOOL _initalConfigurationDone;
}

- (instancetype)initWithStyle:(ChromeButtonStyle)style {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self setupInitialConfiguration];
    self.style = style;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.pointerInteractionEnabled = YES;
    self.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();
    _initalConfigurationDone = YES;
  }
  return self;
}

#pragma mark - Properties

- (void)setStyle:(ChromeButtonStyle)style {
  if (_initalConfigurationDone && _style == style) {
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
  UIButtonConfiguration* button_configuration = self.configuration;
  button_configuration.title = title;
  self.configuration = button_configuration;
}

- (NSString*)title {
  return self.configuration.title;
}

- (void)setFont:(UIFont*)font {
  UIFont* current_font = self.font;
  if ([current_font isEqual:font]) {
    return;
  }

  UIConfigurationTextAttributesTransformer original_transformer =
      self.configuration.titleTextAttributesTransformer;
  self.configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSDictionary<NSAttributedStringKey, id>* transformed =
        original_transformer(incoming);
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [transformed mutableCopy];
    outgoing[NSFontAttributeName] = font;
    return outgoing;
  };
}

- (UIFont*)font {
  NSDictionary<NSAttributedStringKey, id>* attributes =
      self.configuration.titleTextAttributesTransformer(@{});
  return attributes[NSFontAttributeName];
}

- (void)setPrimaryButtonImage:(PrimaryButtonImage)primaryButtonImage {
  CHECK(self.style == ChromeButtonStylePrimary ||
        self.style == ChromeButtonStylePrimaryDestructive);
  UIButtonConfiguration* button_configuration = self.configuration;
  button_configuration.image = nil;
  button_configuration.showsActivityIndicator = NO;
  switch (primaryButtonImage) {
    case PrimaryButtonImageNone:
      break;
    case PrimaryButtonImageSpinner:
      button_configuration.showsActivityIndicator = YES;
      button_configuration.activityIndicatorColorTransformer =
          ^UIColor*(UIColor* _) {
            return UIColor.whiteColor;
          };
      break;
    case PrimaryButtonImageCheckmark: {
      button_configuration.image = CheckmarkImage();
      UIColor* color = UIColor.whiteColor;
      if (self.state & UIControlStateTunedDown) {
        if (self.style == ChromeButtonStylePrimary) {
          color = [UIColor colorNamed:kBlue700Color];
        } else if (self.style == ChromeButtonStylePrimaryDestructive) {
          color = [UIColor colorNamed:kRed600Color];
        }
      }
      button_configuration.imageColorTransformer = ^UIColor*(UIColor* _) {
        return color;
      };
      break;
    }
  }
  self.configuration = button_configuration;
}

- (PrimaryButtonImage)primaryButtonImage {
  if (self.configuration.showsActivityIndicator) {
    return PrimaryButtonImageSpinner;
  }
  if (self.configuration.image) {
    return PrimaryButtonImageCheckmark;
  }
  return PrimaryButtonImageNone;
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
    if ([UIButtonConfiguration
            respondsToSelector:@selector(prominentGlassButtonConfiguration)]) {
      self.configuration =
          [UIButtonConfiguration prominentGlassButtonConfiguration];
    } else {
      self.configuration = [UIButtonConfiguration glassButtonConfiguration];
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
  UIColor* enabled_text_color = [UIColor colorNamed:kSolidButtonTextColor];
  UIColor* disabled_text_color = [UIColor colorNamed:kSolidBlackColor];
  SetButtonTitleTextAttributes(configuration, font, enabled_text_color, self,
                               disabled_text_color);
  configuration.baseForegroundColor = enabled_text_color;
  if (@available(iOS 26, *)) {
    configuration.background.backgroundColor = UIColor.clearColor;
    self.tintColor = [UIColor colorNamed:kBlueColor];
  } else {
    configuration.background.backgroundColor = [UIColor colorNamed:kBlueColor];
  }
  self.configuration = configuration;
  self.configurationUpdateHandler =
      PrimaryActionConfigurationUpdateHandler(/*destructive=*/false);
}

// Updates `button` to match a primary destruction action style.
- (void)updateButtonToMatchPrimaryDestructiveAction {
  UIButtonConfiguration* configuration = self.configuration;
  UIColor* enabled_text_color = [UIColor colorNamed:kSolidButtonTextColor];
  UIColor* disabled_text_color = [UIColor colorNamed:kSolidBlackColor];
  SetButtonTitleTextAttributes(
      configuration, [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline],
      enabled_text_color, self, disabled_text_color);
  configuration.baseForegroundColor = enabled_text_color;
  if (@available(iOS 26, *)) {
    configuration.background.backgroundColor = UIColor.clearColor;
    self.tintColor = [UIColor colorNamed:kRedColor];
  } else {
    configuration.background.backgroundColor = [UIColor colorNamed:kRedColor];
  }
  self.configuration = configuration;
  self.configurationUpdateHandler =
      PrimaryActionConfigurationUpdateHandler(/*destructive=*/true);
}

// Updates `button` to match a secondary action style.
- (void)updateButtonToMatchSecondaryAction {
  UIButtonConfiguration* configuration = self.configuration;
  UIColor* enabled_text_color;
  UIFont* font;
  if (@available(iOS 26, *)) {
    enabled_text_color = [UIColor colorNamed:kSolidBlackColor];
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    self.tintColor = UIColor.clearColor;
  } else {
    enabled_text_color = [UIColor colorNamed:kBlueColor];
    font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    configuration.background.backgroundColor = UIColor.clearColor;
  }
  SetButtonTitleTextAttributes(configuration, font, enabled_text_color);
  configuration.baseForegroundColor = enabled_text_color;
  self.configuration = configuration;
  self.configurationUpdateHandler =
      NonPrimaryActionConfigurationUpdateHandler();
}

// Updates `button` to match a tertiary action style.
- (void)updateButtonToMatchTertiaryAction {
  UIButtonConfiguration* configuration = self.configuration;
  UIColor* enabled_text_color = [UIColor colorNamed:kBlueColor];
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  SetButtonTitleTextAttributes(configuration, font, enabled_text_color);
  configuration.baseForegroundColor = enabled_text_color;
  if (@available(iOS 26, *)) {
    configuration.background.backgroundColor = UIColor.clearColor;
    self.tintColor = [UIColor colorNamed:kBlueHaloColor];
  } else {
    configuration.background.backgroundColor =
        [UIColor colorNamed:kBlueHaloColor];
  }
  self.configuration = configuration;
  self.configurationUpdateHandler =
      NonPrimaryActionConfigurationUpdateHandler();
}

@end
