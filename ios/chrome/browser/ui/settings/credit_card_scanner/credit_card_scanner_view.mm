// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_view.h"

#import "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Credit Card aspect ratio.
CGSize CreditCardAspectRatio = CGSizeMake(85.60, 53.98);

// Width and height of the Credit Card scanner viewport.
const CGSize kViewportSizeIPhone =
    CGSizeMake(CreditCardAspectRatio.width * 2.6,
               CreditCardAspectRatio.height * 2.6);
const CGSize kViewportSizeIPad = CGSizeMake(CreditCardAspectRatio.width * 4.8,
                                            CreditCardAspectRatio.height * 4.8);

}  // namespace

@implementation CreditCardScannerView

#pragma mark - ScannerView

- (CGSize)viewportSize {
  return IsIPadIdiom() ? kViewportSizeIPad : kViewportSizeIPhone;
}

- (NSString*)caption {
  return l10n_util::GetNSString(IDS_IOS_CREDIT_CARD_SCANNER_VIEWPORT_CAPTION);
}

@end
