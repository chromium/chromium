// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_new_tab_button.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size of the small symbol image.
const CGFloat kSmallSymbolSize = 24;
// Size of the button when using a large symbol.
const CGFloat kSmallSize = 38;
// The size of the large symbol image.
const CGFloat kLargeSymbolSize = 28;
// Size of the button when using a large symbol.
const CGFloat kLargeSize = 44;
// The size of the large symbol image.
const CGFloat kLargeSymbolSizeIPad = 34;
// Size of the button when using a large symbol.
const CGFloat kLargeSizeIPad = 52;
// The corner radius to display the button in a square container.
const CGFloat kSquareCornerRadius = 10;
}  // namespace

@implementation TabGridNewTabButton {
  // The symbol for this button.
  UIImage* _symbol;
  // The image container, centered with the button. Not using the image of the
  // button to avoid alignment issues.
  UIImageView* _imageContainer;
}

- (instancetype)initWithLargeSize:(BOOL)largeSize {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    CGFloat symbolSize;
    CGFloat buttonSize;
    if (largeSize) {
      if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
        symbolSize = kLargeSymbolSizeIPad;
        buttonSize = kLargeSizeIPad;
      } else {
        symbolSize = kLargeSymbolSize;
        buttonSize = kLargeSize;
      }
    } else {
      symbolSize = kSmallSymbolSize;
      buttonSize = kSmallSize;
    }

    _symbol = CustomSymbolWithPointSize(kPlusCircleFillSymbol, symbolSize);

    if (@available(iOS 26, *)) {
      self.configuration = [UIButtonConfiguration glassButtonConfiguration];
      _symbol = DefaultSymbolWithPointSize(kPlusSymbol, symbolSize);
      self.tintColor = UIColor.blackColor;
    }

    _imageContainer = [[UIImageView alloc] initWithImage:_symbol];
    _imageContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_imageContainer];

    AddSameCenterConstraints(self, _imageContainer);

    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor constraintEqualToConstant:buttonSize],
      [self.widthAnchor constraintEqualToAnchor:self.heightAnchor],
    ]];
    self.pointerInteractionEnabled = YES;
    self.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();
  }
  return self;
}

- (void)setEnabled:(BOOL)enabled {
  [super setEnabled:enabled];
  [self setSymbolPage:self.page];
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  [self setSymbolPage:page];
}

#pragma mark - Private

// Sets page using a symbol image.
- (void)setSymbolPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);

      if (@available(iOS 26, *)) {
        UIButtonConfiguration* config = self.configuration;
        config.background.backgroundColor = UIColor.whiteColor;
        // Set the corner style to display a circle button.
        config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
        self.configuration = config;
      } else {
        _imageContainer.image = SymbolWithPalette(_symbol, @[
          UIColor.blackColor,
          self.enabled ? UIColor.whiteColor : UIColor.whiteColor
        ]);
      }

      break;
    case TabGridPageRegularTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);

      if (@available(iOS 26, *)) {
        UIButtonConfiguration* config = self.configuration;
        config.background.backgroundColor =
            [UIColor colorNamed:kStaticBlue400Color];
        // Set the corner style to display a circle button.
        config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
        self.configuration = config;
      } else {
        _imageContainer.image = SymbolWithPalette(
            _symbol,
            @[ UIColor.blackColor, [UIColor colorNamed:kStaticBlue400Color] ]);
      }

      break;
    case TabGridPageTabGroups:
      if (base::FeatureList::IsEnabled(kTabRecallNewTabGroupButton)) {
        self.accessibilityLabel =
            l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB_GROUP);

        if (@available(iOS 26, *)) {
          UIButtonConfiguration* config = self.configuration;
          config.background.backgroundColor =
              [UIColor colorNamed:kStaticBlue400Color];
          // Set the corner style and radius to display a square button.
          config.cornerStyle = UIButtonConfigurationCornerStyleFixed;
          config.background.cornerRadius = kSquareCornerRadius;
          self.configuration = config;
        } else {
          _imageContainer.image = SymbolWithPalette(_symbol, @[
            UIColor.blackColor, [UIColor colorNamed:kStaticBlue400Color]
          ]);
        }
      }
      break;
  }
  _page = page;
}

@end
