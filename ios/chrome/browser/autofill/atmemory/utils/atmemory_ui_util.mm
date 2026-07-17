// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"

#import <algorithm>

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// Size of the close button.
constexpr CGFloat kCloseButtonSize = 44;

// Size of the close button when liquid glass is disabled.
constexpr CGFloat kCloseButtonSizePreLiquidGlass = 30;

// Size of the cell icon.
constexpr CGFloat kCellIconSize = 24;
}  // namespace

namespace autofill {

// Returns the symbol configuration to use for the close button.
UIImageSymbolConfiguration* GetCloseButtonSymbolConfiguration() {
  if (@available(iOS 26, *)) {
    return [UIImageSymbolConfiguration
        configurationWithPointSize:kCloseButtonSize
                            weight:UIImageSymbolWeightThin
                             scale:UIImageSymbolScaleDefault];
  }

  return [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseButtonSizePreLiquidGlass
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
}

// Returns the foreground color to use for the close button color palette.
UIColor* GetCloseButtonForegroundColor() {
  if (@available(iOS 26, *)) {
    return [UIColor colorNamed:kTextPrimaryColor];
  }

  return [[UIColor secondaryLabelColor] colorWithAlphaComponent:0.6];
}

ImageContentConfiguration* AtMemoryCellIconConfiguration(
    NSString* symbol_name) {
  UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCellIconSize
                          weight:UIImageSymbolWeightMedium
                           scale:UIImageSymbolScaleMedium];
  UIImage* symbol;
  if ([UIImage imageNamed:symbol_name]) {
    symbol = CustomSymbolWithConfiguration(symbol_name, configuration);
  } else {
    symbol = DefaultSymbolWithConfiguration(symbol_name, configuration);
  }

  ImageContentConfiguration* symbol_configuration =
      [[ImageContentConfiguration alloc] init];
  symbol_configuration.image = symbol;
  symbol_configuration.imageTintColor = [UIColor colorNamed:kTextPrimaryColor];
  return symbol_configuration;
}

}  // namespace autofill
