// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/scanner/scanner_view_controller.h"
#include "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_camera_controller.h"

extern NSString* const kCreditCardScannerViewID;

@protocol CreditCardScannedImageDelegate;
@protocol LoadQueryCommands;

// View controller for the Credit Card Scanner
@interface CreditCardScannerViewController
    : ScannerViewController <CreditCardScannerCameraControllerDelegate>

// Arguments |presentationProvider| and |delegate| should not be nil.
- (instancetype)
    initWithPresentationProvider:(id<ScannerPresenting>)presentationProvider
                        delegate:(id<CreditCardScannedImageDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithPresentationProvider:
                    (id<ScannerPresenting>)presentationProvider
                                 queryLoader:(id<LoadQueryCommands>)queryLoader
    NS_UNAVAILABLE;

- (instancetype)initWithPresentationProvider:
    (id<ScannerPresenting>)presentationProvider NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_VIEW_CONTROLLER_H_
