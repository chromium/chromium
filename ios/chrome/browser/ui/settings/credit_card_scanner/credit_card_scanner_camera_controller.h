// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_CAMERA_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_CAMERA_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_camera_controller_delegate.h"

@class CameraController;

// The CreditCardScannerCameraController manages the AVCaptureSession, its
// inputs, outputs, and notifications for the CreditCardScannerCameraController.
@interface CreditCardScannerCameraController : CameraController

// Initializes the Credit Card scanner delegate.
- (instancetype)initWithCreditCardScannerDelegate:
    (id<CreditCardScannerCameraControllerDelegate>)creditCardScannerDelegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithDelegate:(id<CameraControllerDelegate>)delegate
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_CAMERA_CONTROLLER_H_
