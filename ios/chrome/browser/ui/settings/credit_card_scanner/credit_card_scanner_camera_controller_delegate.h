// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_CAMERA_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_CAMERA_CONTROLLER_DELEGATE_H_

#import "ios/chrome/browser/ui/scanner/camera_controller.h"

// Receives the Credit Card scanner image results.
@protocol CreditCardScannerCameraControllerDelegate <CameraControllerDelegate>

// Called when the scanner starts receiving video frames through the camera
// video input.
- (void)receiveCreditCardScannerResult:(CMSampleBufferRef)sampleBuffer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_CAMERA_CONTROLLER_DELEGATE_H_
