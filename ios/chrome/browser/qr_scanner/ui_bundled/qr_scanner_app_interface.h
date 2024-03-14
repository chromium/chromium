// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_APP_INTERFACE_H_

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/scanner/camera_state.h"

@interface QRScannerAppInterface : NSObject

// Returns the current BrowserViewController. This is used throughout the
// tests to get the currently-presented modal view controller (usually the
// QRScannerViewController).
@property(nonatomic, class, readonly)
    UIViewController* currentBrowserViewController;

// Returns the accessibility used for the close button icon.
@property(nonatomic, class, readonly) NSString* closeIconAccessibilityLabel;

#pragma mark Swizzling

// Returns the block to use for swizzling the QRScannerViewController property
// cameraController: to return `cameraControllerMock` instead of a new instance
// of CameraController.
// This block is only used for swizzling, which is why its type is opaque. It
// should not be called directly.
+ (id)cameraControllerSwizzleBlockWithMock:(id)cameraControllerMock;

#pragma mark Mocking and Expectations

// Creates a new CameraController mock with `authorizationStatus` set.
+ (id)cameraControllerMockWithAuthorizationStatus:
    (AVAuthorizationStatus)authorizationStatus;

// Adds functions which are expected to be called when the
// QRScannerViewController is presented to `cameraControllerMock`.
+ (void)addCameraControllerInitializationExpectations:(id)cameraControllerMock;

// Adds functions which are expected to be called when the
// QRScannerViewController is dismissed to `cameraControllerMock`.
+ (void)addCameraControllerDismissalExpectations:(id)cameraControllerMock;

// Adds functions which are expected to be called when the torch is switched on
// to `cameraControllerMock`.
+ (void)addCameraControllerTorchOnExpectations:(id)cameraControllerMock;

// Adds functions which are expected to be called when the torch is switched off
// to `cameraControllerMock`.
+ (void)addCameraControllerTorchOffExpectations:(id)cameraControllerMock;

#pragma mark CameraControllerDelegate calls

// Calls `cameraStateChanged:` on the presented QRScannerViewController.
+ (void)callCameraStateChanged:(scanner::CameraState)state;

// Calls `torchStateChanged:` on the presented QRScannerViewController.
+ (void)callTorchStateChanged:(BOOL)torchIsOn;

// Calls `torchAvailabilityChanged:` on the presented QRScannerViewController.
+ (void)callTorchAvailabilityChanged:(BOOL)torchIsAvailable;

// Calls `receiveQRScannerResult:` on the presented QRScannerViewController.
+ (void)callReceiveQRScannerResult:(NSString*)result;

#pragma mark Modal helpers for dialogs

// Checks that the modal presented by `viewController` is of class
// `className`. Returns nil if the check passes and an NSError otherwise.
+ (NSError*)assertModalOfClass:(NSString*)className
                 isPresentedBy:(UIViewController*)viewController
    __attribute__((warn_unused_result));

// Checks that the `viewController` is not presenting a modal, or that the modal
// presented by `viewController` is not of class `className`.  Returns nil if
// the check passes and an NSError otherwise.
+ (NSError*)assertModalOfClass:(NSString*)className
              isNotPresentedBy:(UIViewController*)viewController
    __attribute__((warn_unused_result));

// Returns a block that checks that the `viewController` is not presenting a
// modal, or that the modal presented by `viewController` is not of class
// `className`. This block can be waited on in the test process.
+ (BOOL (^)())blockForWaitingForModalOfClass:(NSString*)className
                        toDisappearFromAbove:(UIViewController*)viewController;

// Returns the expected title for the dialog which is presented for `state`.
+ (NSString*)dialogTitleForState:(scanner::CameraState)state;

#pragma mark VoiceOver overrides

// Overrides the VoiceOver check for `qrScanner` to `isOn`. `qrScanner` should
// be an instance of QRScannerViewController. Calls to this method should also
// be paired, so the VoiceOver status is left in the same state as it started.
+ (void)overrideVoiceOverCheckForQRScannerViewController:
            (UIViewController*)qrScanner
                                                    isOn:(BOOL)isOn;

// Posts a fake VoiceOver end announcement.
+ (void)postScanEndVoiceoverAnnouncement;

@end

#endif  // IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_APP_INTERFACE_H_
