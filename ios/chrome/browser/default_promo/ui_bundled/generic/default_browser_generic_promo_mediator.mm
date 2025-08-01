// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_mediator.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation DefaultBrowserGenericPromoMediator

#pragma mark - Public

- (void)didTapPrimaryActionButton {
  [self didTapPrimaryActionButton:/*useDefaultAppsDestination=*/NO];
}

- (void)didTapPrimaryActionButton:(BOOL)useDefaultAppsDestination {
  NSURL* url = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
#if defined(__IPHONE_18_3) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_18_3
  if (@available(iOS 18.3, *)) {
    if (useDefaultAppsDestination) {
      url = [NSURL
          URLWithString:UIApplicationOpenDefaultApplicationsSettingsURLString];
    }
  }
#endif
  [[UIApplication sharedApplication] openURL:url
                                     options:{}
                           completionHandler:nil];
}

@end
