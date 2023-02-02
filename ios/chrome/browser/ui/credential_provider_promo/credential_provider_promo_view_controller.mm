// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_view_controller.h"

#import "base/notreached.h"
#import "base/values.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CredentialProviderPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  // TODO(crbug.com/1392116): configure the action sheet.
  NOTREACHED();
}

#pragma mark - CredentialProviderPromoConsumer

- (void)setTitleString:(NSString*)titleString
           subtitleString:(NSString*)subtitleString
      primaryActionString:(NSString*)primaryActionString
    secondaryActionString:(NSString*)secondaryActionString
     tertiaryActionString:(NSString*)tertiaryActionString
                    image:(UIImage*)image {
  _alertScreen.titleString = titleString;
  _alertScreen.subtitleString = subtitleString;
  _alertScreen.primaryActionString = primaryActionString;
  _alertScreen.secondaryActionString = secondaryActionString;
  _alertScreen.tertiaryActionString = tertiaryActionString;
  _alertScreen.image = image;
}

- (void)setAnimation:(std::unique_ptr<base::Value>)animationAsset {
  // TODO(crbug.com/1392116): configure animation view for full-screen
  // promo.
  NOTREACHED();
}

@end
