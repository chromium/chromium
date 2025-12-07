// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_CAMERA_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_CAMERA_CONTROLLER_H_

#import "ios/chrome/browser/scanner/ui_bundled/camera_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_camera_controller_delegate.h"

// The CreditCardScannerCameraController manages the AVCaptureSession, its
// inputs, outputs, and notifications for the CreditCardScannerCameraController.
@interface CreditCardScannerCameraController : CameraController

// Initializes the Credit Card scanner delegate.
- (instancetype)initWithCreditCardScannerDelegate:
    (id<CreditCardScannerCameraControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithDelegate:(id<CameraControllerDelegate>)delegate
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_CAMERA_CONTROLLER_H_
