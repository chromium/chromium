// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_camera_controller.h"

#import <CoreVideo/CoreVideo.h>

@interface CreditCardScannerCameraController () <
    AVCaptureVideoDataOutputSampleBufferDelegate>

@end

@implementation CreditCardScannerCameraController {
  // The delegate which receives the Credit Card scanned result.
  __weak id<CreditCardScannerCameraControllerDelegate>
      _creditCardScannerDelegate;
}

#pragma mark - Lifecycle

- (instancetype)initWithCreditCardScannerDelegate:
    (id<CreditCardScannerCameraControllerDelegate>)delegate {
  self = [super initWithDelegate:delegate];
  if (self) {
    _creditCardScannerDelegate = delegate;
  }
  return self;
}

#pragma mark - CameraController

- (void)configureScannerWithSession:(AVCaptureSession*)session {
  // Configure camera output.
  NSDictionary* outputSettings = @{
    (__bridge NSString*)kCVPixelBufferPixelFormatTypeKey :
        [NSNumber numberWithInteger:kCVPixelFormatType_32BGRA]
  };
  AVCaptureVideoDataOutput* videoOutput =
      [[AVCaptureVideoDataOutput alloc] init];

  videoOutput.videoSettings = outputSettings;

  [videoOutput setSampleBufferDelegate:self queue:self.sessionQueue];

  videoOutput.alwaysDiscardsLateVideoFrames = YES;

  [session addOutput:videoOutput];
}

#pragma mark AVCaptureMetadataOutputObjectsDelegate

- (void)captureOutput:(AVCaptureOutput*)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection {
  [_creditCardScannerDelegate receiveCreditCardScannerResult:sampleBuffer];
}

@end
