// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/ntp_tools_menu_button.h"

#import "ios/chrome/browser/content_suggestions/public/ntp_home_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/shared/ui/elements/blue_dot_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation NTPToolsMenuButton {
  UIView* _blueDotView;
  UIView* _customBackgroundView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.accessibilityIdentifier = kNTPToolsMenuButtonIdentifier;
    self.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU);
    self.pointerInteractionEnabled = YES;

    UIButtonConfiguration* configuration =
        [UIButtonConfiguration plainButtonConfiguration];
    UIImage* icon = SymbolTemplateWithPointSize(
        SymbolMenu, ntp_home::kNTPMenuButtonIconSize);
    configuration.image = icon;
    configuration.background.cornerRadius =
        ntp_home::kNTPMenuButtonCornerRadius;
    self.configuration = configuration;

    UIColor* unthemedTintColor =
        IsNewTabPageUICleanupEnabled()
            ? [UIColor colorNamed:kNTPRedesignCustomizationMenuButtonIconColor]
            : [UIColor colorNamed:kBlue600Color];
    self.configurationUpdateHandler =
        CreateThemedButtonConfigurationUpdateHandler(
            unthemedTintColor, ^UIColor*(NewTabPageColorPalette* palette) {
              if (palette) {
                return palette.headerButtonColor;
              }

              return [UIColor colorWithDynamicProvider:^UIColor*(
                                  UITraitCollection* traits) {
                if (traits.userInterfaceStyle == UIUserInterfaceStyleDark) {
                  return IsNewTabPageUICleanupEnabled()
                             ? [UIColor colorNamed:kSurfaceContainerLowColor]
                             : [UIColor
                                   colorNamed:kTabGroupFaviconBackgroundColor];
                }
                return [[UIColor colorNamed:kSolidWhiteColor]
                    colorWithAlphaComponent:
                        ntp_home::kNTPMenuButtonLightUnthemedAlpha];
              }];
            });
  }
  return self;
}

- (void)setBlueDot:(BOOL)blueDot {
  if (_blueDot == blueDot) {
    return;
  }
  _blueDot = blueDot;

  if (blueDot && !_blueDotView) {
    _blueDotView = ConfigureAndAddBlueDotView(self);
  }
  _blueDotView.hidden = !blueDot;

  self.accessibilityValue =
      blueDot ? l10n_util::GetNSString(IDS_IOS_NEW_ITEM_ACCESSIBILITY_HINT)
              : nil;
  [self setNeedsLayout];
}

#pragma mark - UIButton

// Wrap the configuration's custom background view in our own view so
// that we can apply a mask to it (for the blue dot hole) without clipping
// the blue dot itself, which is added as a subview to the button.
- (void)setConfiguration:(UIButtonConfiguration*)configuration {
  UIButtonConfiguration* configCopy = [configuration copy];

  if (!_customBackgroundView) {
    _customBackgroundView = [[UIView alloc] init];
    _customBackgroundView.userInteractionEnabled = NO;
  }

  UIView* providedCustomView = configCopy.background.customView;

  // If the config has a custom view that isn't already the wrapper, or if it
  // has no custom view at all, update the wrapper's contents.
  if (providedCustomView != _customBackgroundView) {
    [_customBackgroundView.subviews
        makeObjectsPerformSelector:@selector(removeFromSuperview)];

    if (providedCustomView) {
      providedCustomView.translatesAutoresizingMaskIntoConstraints = NO;
      [_customBackgroundView addSubview:providedCustomView];
      AddSameConstraints(providedCustomView, _customBackgroundView);
      // Force layout immediately without animation so the UIVisualEffectView
      // snaps to full size instantly instead of growing from CGRectZero.
      [UIView performWithoutAnimation:^{
        [_customBackgroundView layoutIfNeeded];
      }];
    }
  }

  _customBackgroundView.backgroundColor = configCopy.background.backgroundColor;
  configCopy.background.backgroundColor = [UIColor clearColor];
  configCopy.background.customView = _customBackgroundView;

  [super setConfiguration:configCopy];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  // Apply the mask to our custom background view, so the hole is punched
  // in the color pill without clipping the blue dot (which is a subview of
  // self).
  if (_customBackgroundView) {
    UpdateBlueDotMaskForView(_customBackgroundView, _blueDot);
  }
}

@end
