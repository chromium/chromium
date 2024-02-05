// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_VIEW_CONTROLLER_H_

#include "ios/chrome/browser/qr_scanner/ui_bundled/qr_scanner_camera_controller.h"
#import "ios/chrome/browser/ui/scanner/scanner_view_controller.h"

@protocol LoadQueryCommands;

// View controller for the QR Scanner.
@interface QRScannerViewController
    : ScannerViewController <QRScannerCameraControllerDelegate>

- (instancetype)initWithPresentationProvider:
                    (id<ScannerPresenting>)presentationProvider
                                 queryLoader:(id<LoadQueryCommands>)queryLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithPresentationProvider:
    (id<ScannerPresenting>)presentationProvider NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Returns a view controller to be presented based on the camera state. Returns
// `self` if the camera is available or an appropriate UIAlertController if
// there was an error loading the camera.
- (UIViewController*)viewControllerToPresent;

@end

@interface QRScannerViewController (TestingAdditions)

// Simulates VoiceOver being enabled for this Scanner.
- (void)overrideVoiceOverCheck:(BOOL)overrideVoiceOverCheck;

@end

#endif  // IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_VIEW_CONTROLLER_H_
