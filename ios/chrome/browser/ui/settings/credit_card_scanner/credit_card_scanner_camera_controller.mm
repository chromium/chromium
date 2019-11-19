// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_camera_controller.h"

#import <CoreVideo/CoreVideo.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CreditCardScannerCameraController () <
    AVCaptureVideoDataOutputSampleBufferDelegate>

// The delegate which receives the Credit Card scanned result.
@property(nonatomic, readwrite, weak)
    id<CreditCardScannerCameraControllerDelegate>
        creditCardScannerDelegate;

@end

@implementation CreditCardScannerCameraController

#pragma mark - Lifecycle

- (instancetype)initWithCreditCardScannerDelegate:
    (id<CreditCardScannerCameraControllerDelegate>)creditCardScannerDelegate {
  self = [super initWithDelegate:creditCardScannerDelegate];
  if (self) {
    _creditCardScannerDelegate = creditCardScannerDelegate;
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
  [self.creditCardScannerDelegate receiveCreditCardScannerResult:sampleBuffer];
}

@end
