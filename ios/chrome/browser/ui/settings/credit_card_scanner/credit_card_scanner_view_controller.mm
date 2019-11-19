// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_view_controller.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanned_image_delegate.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_camera_controller.h"
#include "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

NSString* const kCreditCardScannerViewID = @"kCreditCardScannerViewID";

@interface CreditCardScannerViewController ()

// The delegate notified when there is a new image from the the scanner.
@property(nonatomic, weak) id<CreditCardScannedImageDelegate> delegate;

// The rect storing the location of the credit card scanner viewport to set the
// region of interest of the Vision request.
// This is needed so that UI elements stay on the main thread and
// |creditCardViewport| can be sent as parameter through the
// CreditCardScannerCameraControllerDelegate from the background thread.
@property(nonatomic, assign) CGRect creditCardViewport;

@end

@implementation CreditCardScannerViewController

#pragma mark Lifecycle

- (instancetype)
    initWithPresentationProvider:(id<ScannerPresenting>)presentationProvider
                        delegate:(id<CreditCardScannedImageDelegate>)delegate {
  self = [super initWithPresentationProvider:presentationProvider];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kCreditCardScannerViewID;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Crop the scanner subviews to the size of the |scannerView| to avoid the
  // preview overlay being visible outside the screen bounds while presenting.
  self.scannerView.clipsToBounds = YES;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // Allow the constraints to modify the preview overlay size to cover the
  // entire screen during rotation.
  self.scannerView.clipsToBounds = NO;
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  // Crop the scanner subviews to the size of the |scannerView| to avoid the
  // preview overlay being visible outside the screen bounds while dismissing.
  self.scannerView.clipsToBounds = YES;
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.creditCardViewport = [self.scannerView viewportRegionOfInterest];
}

#pragma mark - ScannerViewController

- (ScannerView*)buildScannerView {
  return [[CreditCardScannerView alloc] initWithFrame:self.view.frame
                                             delegate:self];
}

- (CameraController*)buildCameraController {
  return [[CreditCardScannerCameraController alloc]
      initWithCreditCardScannerDelegate:self];
}

- (void)dismissForReason:(scannerViewController::DismissalReason)reason
          withCompletion:(void (^)(void))completion {
  switch (reason) {
    case scannerViewController::CLOSE_BUTTON:
      base::RecordAction(UserMetricsAction("MobileCreditCardScannerClose"));
      break;
    case scannerViewController::ERROR_DIALOG:
      base::RecordAction(UserMetricsAction("MobileCreditCardScannerError"));
      break;
    case scannerViewController::SCAN_COMPLETE:
      // Fall through.
    case scannerViewController::IMPOSSIBLY_UNLIKELY_AUTHORIZATION_CHANGE:
      break;
  }

  [super dismissForReason:reason withCompletion:completion];
}

#pragma mark - CreditCardScannerCameraControllerDelegate

- (void)receiveCreditCardScannerResult:(CMSampleBufferRef)sampleBuffer {
  [self.delegate processOutputSampleBuffer:sampleBuffer
                                  viewport:self.creditCardViewport];
}

@end
