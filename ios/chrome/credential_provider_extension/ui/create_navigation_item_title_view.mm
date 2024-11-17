// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/create_navigation_item_title_view.h"

#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"

namespace {

// Point size of the SF Symbol used for the logo of the
// BrandedNavigationItemTitleView.
const CGFloat kSymbolPointSize = 17.0;

// Horizontal spacing between the logo and the title label of the
// BrandedNavigationItemTitleView.
const CGFloat kHorizontalSpacing = 9.0;

// Name of the symbol presented with the navigation item title view.
NSString* const kMulticolorCredentialSymbol = @"multicolor_credential";

}  // namespace

namespace credential_provider_extension {

UIView* CreateNavigationItemTitleView(UIFont* font) {
  BrandedNavigationItemTitleView* titleView =
      [[BrandedNavigationItemTitleView alloc] initWithFont:font];

  NSString* titleString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_BRANDED_TITLE", @"Password Manager");
  titleView.title = titleString;
  titleView.accessibilityLabel = titleString;

  UIImage* symbol =
      [UIImage imageNamed:kMulticolorCredentialSymbol
                   inBundle:nil
          withConfiguration:
              [UIImageSymbolConfiguration
                  configurationWithPointSize:kSymbolPointSize
                                      weight:UIImageSymbolWeightMedium
                                       scale:UIImageSymbolScaleMedium]];
  titleView.imageLogo = [symbol
      imageByApplyingSymbolConfiguration:
          [UIImageSymbolConfiguration configurationPreferringMulticolor]];

  titleView.titleLogoSpacing = kHorizontalSpacing;

  return titleView;
}

}  // namespace credential_provider_extension
