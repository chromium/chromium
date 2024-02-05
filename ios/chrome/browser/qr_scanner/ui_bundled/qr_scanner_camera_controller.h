// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_CAMERA_CONTROLLER_H_
#define IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_CAMERA_CONTROLLER_H_

#import "ios/chrome/browser/qr_scanner/ui_bundled/qr_scanner_camera_controller_delegate.h"

@class CameraController;

// The QRScannerCameraController manages the AVCaptureSession, its inputs,
// outputs, and notifications for the QRScannerViewController.
@interface QRScannerCameraController : CameraController

// Initializes the QR scanner delegate.
- (instancetype)initWithQRScannerDelegate:
    (id<QRScannerCameraControllerDelegate>)qrScannerDelegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithDelegate:(id<CameraControllerDelegate>)delegate
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_CAMERA_CONTROLLER_H_
