// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_view_controller.h"

NSString* const kCreditCardScannerViewID = @"kCreditCardScannerViewID";

@interface CreditCardScannerViewController ()

@end

@implementation CreditCardScannerViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kCreditCardScannerViewID;
}

@end
