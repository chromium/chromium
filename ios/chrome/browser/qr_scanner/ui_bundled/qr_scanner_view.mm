// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/qr_scanner/ui_bundled/qr_scanner_view.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Width and height of the QR scanner viewport.
constexpr CGSize kViewportSizeIPhone = CGSize{.width = 250, .height = 250};
constexpr CGSize kViewportSizeIPad = CGSize{.width = 300, .height = 300};

}  // namespace

@implementation QRScannerView

#pragma mark - ScannerView

- (CGSize)viewportSize {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
             ? kViewportSizeIPad
             : kViewportSizeIPhone;
}

- (NSString*)caption {
  return l10n_util::GetNSString(IDS_IOS_QR_SCANNER_VIEWPORT_CAPTION);
}

@end
