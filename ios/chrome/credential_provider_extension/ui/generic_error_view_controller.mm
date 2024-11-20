// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/generic_error_view_controller.h"

namespace {

// Point size of the SF Symbol used for the logo.
const CGFloat kSymbolPointSize = 60.0;

// Name of the symbol presented with the view.
NSString* const kErrorIconSymbol = @"exclamationmark.circle.fill";

}  // namespace

@implementation GenericErrorViewController

#pragma mark - UIViewController

- (void)loadView {
  UIImage* symbol =
      [UIImage systemImageNamed:kErrorIconSymbol
              withConfiguration:
                  [UIImageSymbolConfiguration
                      configurationWithPointSize:kSymbolPointSize
                                          weight:UIImageSymbolWeightMedium
                                           scale:UIImageSymbolScaleMedium]];
  self.image = [symbol
      imageByApplyingSymbolConfiguration:
          [UIImageSymbolConfiguration
              configurationWithHierarchicalColor:UIColor.secondaryLabelColor]];
  self.imageHasFixedSize = YES;
  self.titleString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_NEW_PASSWORD_ERROR_MESSAGE",
      @"A generic error message.");

  [super loadView];
}

@end
