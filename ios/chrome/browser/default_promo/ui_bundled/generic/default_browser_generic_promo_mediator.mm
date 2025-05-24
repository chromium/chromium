// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_mediator.h"

#import "ui/base/l10n/l10n_util_mac.h"

@implementation DefaultBrowserGenericPromoMediator

#pragma mark - Public

- (void)didTapPrimaryActionButton {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
}

@end
