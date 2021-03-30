
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_generator/qr_generator_view_controller.h"

#import "ios/chrome/browser/ui/qr_generator/qr_generator_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Height and width of the QR code image, in points.
const CGFloat kQRCodeImageSize = 200.0;

@interface QRGeneratorViewController ()

@end

@implementation QRGeneratorViewController

#pragma mark - UIViewController

- (void)loadView {
  self.image = [self createQRCodeImage];
  self.imageAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_QR_CODE_ACCESSIBILITY_LABEL);
  self.imageHasFixedSize = YES;

  self.titleTextStyle = UIFontTextStyleTitle3;

  self.subtitleString = [self.pageURL host];

  self.primaryActionAvailable = YES;
  self.primaryActionString = l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL);

  self.helpButtonAvailable = YES;
  self.helpButtonAccessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HELP_ACCESSIBILITY_LABEL);

  self.alwaysShowImage = YES;
  self.primaryActionBarButtonStyle = UIBarButtonSystemItemAction;

  if (@available(iOS 13.4, *)) {
      self.pointerInteractionEnabled = YES;
  }

  [super loadView];
}

#pragma mark - Private Methods

- (UIImage*)createQRCodeImage {
  NSData* urlData =
      [[self.pageURL absoluteString] dataUsingEncoding:NSUTF8StringEncoding];
  return GenerateQRCode(urlData, kQRCodeImageSize);
}

@end
