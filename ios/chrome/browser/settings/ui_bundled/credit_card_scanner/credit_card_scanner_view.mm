// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_view.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Credit Card aspect ratio.
constexpr CGSize CreditCardAspectRatio = {85.60, 53.98};

// Width and height of the Credit Card scanner viewport.
constexpr CGSize kViewportSizeIPhone = {CreditCardAspectRatio.width * 2.6,
                                        CreditCardAspectRatio.height * 2.6};
constexpr CGSize kViewportSizeIPad = {CreditCardAspectRatio.width * 4.8,
                                      CreditCardAspectRatio.height * 4.8};

}  // namespace

@implementation CreditCardScannerView

#pragma mark - ScannerView

- (CGSize)viewportSize {
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? kViewportSizeIPad
             : kViewportSizeIPhone;
}

- (NSString*)caption {
  return l10n_util::GetNSString(IDS_IOS_CREDIT_CARD_SCANNER_VIEWPORT_CAPTION);
}

@end
