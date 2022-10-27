// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/settings/password/branded_navigation_item_title_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Point size of the SF Symbol used for the logo.
const CGFloat kSymbolPointSize = 17.0;

// Symbol name for the logo image.
// Using password logo as placeholder until Password Manager SF Symbol is added.
NSString* const kLogoSymbolName = kPasswordManagerSymbol;

}  // namespace

namespace password_manager {

BrandedNavigationItemTitleView* CreatePasswordManagerTitleView(
    NSString* title) {
  BrandedNavigationItemTitleView* title_view =
      [[BrandedNavigationItemTitleView alloc] init];

  title_view.title = title;

  title_view.imageLogo = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kLogoSymbolName, kSymbolPointSize));

  title_view.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_MANAGER_TITLE_VIEW_ACCESSIBILITY_LABEL);

  return title_view;
}

}  // namespace password_manager
