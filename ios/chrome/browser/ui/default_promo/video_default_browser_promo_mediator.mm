// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_mediator.h"

#import "base/metrics/user_metrics.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation VideoDefaultBrowserPromoMediator

#pragma mark - Public

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/1446316): Record user action
  [self openSettingsURLString];
}

- (void)didTapSecondaryActionButton {
  // TODO(crbug.com/1446316): Record user action
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
