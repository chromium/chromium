// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_camera_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface QRScannerCameraController () <AVCaptureMetadataOutputObjectsDelegate>

// The delegate which receives the QR scanned result.
@property(nonatomic, weak) id<QRScannerCameraControllerDelegate>
    qrScannerDelegate;

@end

@implementation QRScannerCameraController

#pragma mark - Lifecycle

- (instancetype)initWithQRScannerDelegate:
    (id<QRScannerCameraControllerDelegate>)qrScannerDelegate {
  self = [super initWithDelegate:qrScannerDelegate];
  if (self) {
    _qrScannerDelegate = qrScannerDelegate;
  }
  return self;
}

#pragma mark - CameraController

- (void)configureScannerWithSession:(AVCaptureSession*)session {
  // Configure metadata output.
  AVCaptureMetadataOutput* metadataOutput =
      [[AVCaptureMetadataOutput alloc] init];
  [metadataOutput setMetadataObjectsDelegate:self
                                       queue:dispatch_get_main_queue()];
  if (![session canAddOutput:metadataOutput]) {
    [self setCameraState:scanner::CAMERA_UNAVAILABLE];
    return;
  }
  [session addOutput:metadataOutput];
  NSArray* availableCodeTypes = [metadataOutput availableMetadataObjectTypes];

  // Require QR code recognition to be available.
  if (![availableCodeTypes containsObject:AVMetadataObjectTypeQRCode]) {
    [self setCameraState:scanner::CAMERA_UNAVAILABLE];
    return;
  }
  [metadataOutput setMetadataObjectTypes:availableCodeTypes];
  self.metadataOutput = metadataOutput;
}

#pragma mark - AVCaptureMetadataOutputObjectsDelegate

- (void)captureOutput:(AVCaptureOutput*)captureOutput
    didOutputMetadataObjects:(NSArray*)metadataObjects
              fromConnection:(AVCaptureConnection*)connection {
  AVMetadataObject* metadataResult = [metadataObjects firstObject];
  if (![metadataResult
          isKindOfClass:[AVMetadataMachineReadableCodeObject class]]) {
    return;
  }
  NSString* resultString =
      [base::mac::ObjCCastStrict<AVMetadataMachineReadableCodeObject>(
          metadataResult) stringValue];
  if (resultString.length == 0) {
    return;
  }
  __weak CameraController* weakSelf = self;
  dispatch_async(self.sessionQueue, ^{
    CameraController* strongSelf = weakSelf;
    if (strongSelf && [strongSelf.captureSession isRunning]) {
      [strongSelf.captureSession stopRunning];
    }
  });

  // Check if the barcode can only contain digits. In this case, the result can
  // be loaded immediately.
  NSString* resultType = metadataResult.type;
  BOOL isAllDigits =
      [resultType isEqualToString:AVMetadataObjectTypeUPCECode] ||
      [resultType isEqualToString:AVMetadataObjectTypeEAN8Code] ||
      [resultType isEqualToString:AVMetadataObjectTypeEAN13Code] ||
      [resultType isEqualToString:AVMetadataObjectTypeInterleaved2of5Code] ||
      [resultType isEqualToString:AVMetadataObjectTypeITF14Code];

  // Note: |captureOutput| is called on the main queue. This is specified by
  // |setMetadataObjectsDelegate:queue:|.
  [self.qrScannerDelegate receiveQRScannerResult:resultString
                                 loadImmediately:isAllDigits];
}

@end
