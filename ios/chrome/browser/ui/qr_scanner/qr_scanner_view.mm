// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_view.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Width and height of the QR scanner viewport.
const CGSize kViewportSizeIPhone = CGSizeMake(250, 250);
const CGSize kViewportSizeIPad = CGSizeMake(300, 300);

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
