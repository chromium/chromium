// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/generic/default_browser_generic_promo_mediator.h"

@implementation DefaultBrowserGenericPromoMediator

#pragma mark - Public

- (void)didTapPrimaryActionButton {
  [self openSettingsURLString];
}

#pragma mark Private

// Called to allow the user to open the iOS settings.
- (void)openSettingsURLString {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
}

@end
