// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_VIEW_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_VIEW_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/scanner/ui_bundled/scanner_view.h"

// The view rendering the Credit Card Scanner UI.
//
// This interface specializes ScannerView to define an appropriate viewport
// size for credit cards, and to define a credit card related caption
// instructing the user to position their card within the viewpoit.
@interface CreditCardScannerView : ScannerView

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_VIEW_H_
