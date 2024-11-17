// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/signed_out_user_view_controller.h"

namespace {

// Point size of the SF Symbol used for the logo.
const CGFloat kSymbolPointSize = 60.0;

// Name of the symbol presented with the view.
NSString* const kMulticolorChromeSymbol = @"multicolor_chrome";

}  // namespace

@implementation SignedOutUserViewController

#pragma mark - UIViewController

- (void)loadView {
  UIImage* symbol =
      [UIImage imageNamed:kMulticolorChromeSymbol
                   inBundle:nil
          withConfiguration:
              [UIImageSymbolConfiguration
                  configurationWithPointSize:kSymbolPointSize
                                      weight:UIImageSymbolWeightMedium
                                       scale:UIImageSymbolScaleMedium]];
  self.image = [symbol
      imageByApplyingSymbolConfiguration:
          [UIImageSymbolConfiguration configurationPreferringMulticolor]];
  self.imageHasFixedSize = YES;
  self.titleString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_SIGNED_OUT_USER_TITLE",
                        @"The title in the signed out user screen.");
  self.subtitleString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_SIGNED_OUT_USER_SUBTITLE",
                        @"The subtitle in the signed out user screen.");
  [super loadView];
}

@end
