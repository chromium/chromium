// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_icon_configuration.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {

// Dimensions of the credit card icon.
constexpr CGFloat kCardIconWidth = 40;
constexpr CGFloat kCardIconHeight = 24;

}  // namespace

NSObject<ChromeContentConfiguration>* CreateManualFillIconConfiguration(
    ManualFillIconStyle icon_style,
    UIImage* icon_image,
    FaviconAttributes* favicon_attributes) {
  switch (icon_style) {
    case ManualFillIconStyle::kSymbol: {
      ColorfulSymbolContentConfiguration* symbol_configuration =
          [[ColorfulSymbolContentConfiguration alloc] init];
      symbol_configuration.symbolImage = icon_image;
      symbol_configuration.symbolTintColor =
          [UIColor colorNamed:kTextPrimaryColor];
      // Contrast the icon container against the cell's
      // `kGroupedSecondaryBackgroundColor` background.
      symbol_configuration.symbolBackgroundColor =
          [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
      return symbol_configuration;
    }
    case ManualFillIconStyle::kCardIcon: {
      ImageContentConfiguration* image_configuration =
          [[ImageContentConfiguration alloc] init];
      image_configuration.image = icon_image;
      image_configuration.imageSize =
          CGSizeMake(kCardIconWidth, kCardIconHeight);
      image_configuration.imageContentMode = UIViewContentModeScaleAspectFit;
      return image_configuration;
    }
    case ManualFillIconStyle::kFavicon: {
      // Credential entries without a site favicon (such as passkeys or backup
      // passwords) supply a symbol override.
      if (icon_image) {
        ImageContentConfiguration* image_configuration =
            [[ImageContentConfiguration alloc] init];
        image_configuration.image = icon_image;
        image_configuration.imageSize =
            CGSizeMake(kTableViewIconImageSize, kTableViewIconImageSize);
        image_configuration.imageContentMode = UIViewContentModeCenter;
        image_configuration.imageTintColor =
            [UIColor colorNamed:kTextPrimaryColor];
        return image_configuration;
      }
      FaviconContentConfiguration* favicon_configuration =
          [[FaviconContentConfiguration alloc] init];
      favicon_configuration.faviconAttributes = favicon_attributes;
      return favicon_configuration;
    }
  }
  NOTREACHED();
}
