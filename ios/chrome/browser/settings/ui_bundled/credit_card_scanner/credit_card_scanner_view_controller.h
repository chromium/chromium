// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

extern NSString* const kCreditCardScannerViewID;

@protocol LoadQueryCommands;

// View controller for the Credit Card Scanner
//
// TODO(crbug.com/435324025): Extend and implement ScannerViewController.
@interface CreditCardScannerViewController : UIViewController

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_VIEW_CONTROLLER_H_
