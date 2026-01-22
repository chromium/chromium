// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_QR_SCANNER_UI_QR_SCANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_QR_SCANNER_UI_QR_SCANNER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/qr_scanner/ui/qr_scanner_camera_controller_delegate.h"
#import "ios/chrome/browser/scanner/ui_bundled/scanner_view_controller.h"

// View controller for the QR Scanner.
@interface QRScannerViewController
    : ScannerViewController <QRScannerCameraControllerDelegate>

// Returns a view controller to be presented based on the camera state. Returns
// `self` if the camera is available or an appropriate UIAlertController if
// there was an error loading the camera.
- (UIViewController*)viewControllerToPresent;

@end

@interface QRScannerViewController (TestingAdditions)

// Simulates VoiceOver being enabled for this Scanner.
- (void)overrideVoiceOverCheck:(BOOL)overrideVoiceOverCheck;

@end

#endif  // IOS_CHROME_BROWSER_QR_SCANNER_UI_QR_SCANNER_VIEW_CONTROLLER_H_
