// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_view_controller.h"

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanned_image_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_camera_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

NSString* const kCreditCardScannerViewID = @"kCreditCardScannerViewID";

@implementation CreditCardScannerViewController {
  // The rect storing the location of the credit card scanner viewport to set
  // the region of interest of the Vision request.
  //
  // This is needed so that UI elements stay on the main thread and
  // `creditCardViewport` can be sent as parameter through the
  // CreditCardScannerCameraControllerDelegate from the background thread.
  CGRect _creditCardViewport;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kCreditCardScannerViewID;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Crop the scanner subviews to the size of the `scannerView` to avoid the
  // preview overlay being visible outside the screen bounds while presenting.
  self.scannerView.clipsToBounds = YES;
  [self setupEnterManuallyButton];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // Allow the constraints to modify the preview overlay size to cover the
  // entire screen during rotation.
  self.scannerView.clipsToBounds = NO;
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  // Crop the scanner subviews to the size of the `scannerView` to avoid the
  // preview overlay being visible outside the screen bounds while dismissing.
  self.scannerView.clipsToBounds = YES;
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  _creditCardViewport = [self.scannerView viewportRegionOfInterest];
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

#pragma mark - CreditCardScannerCameraControllerDelegate

- (void)receiveCreditCardScannerResult:(CMSampleBufferRef)sampleBuffer {
  [_delegate processOutputSampleBuffer:sampleBuffer
                              viewport:_creditCardViewport];
}

#pragma mark - Private

- (void)setupEnterManuallyButton {
  UIToolbar* toolbar = nil;
  for (UIView* subview in self.scannerView.subviews) {
    if ([subview isKindOfClass:[UIToolbar class]]) {
      toolbar = (UIToolbar*)subview;
      break;
    }
  }

  if (!toolbar) {
    return;
  }

  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];
  config.title =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SCAN_CARD_ENTER_MANUALLY);
  config.baseForegroundColor = [UIColor colorNamed:kBlueColor];

  UIButton* button = [UIButton buttonWithConfiguration:config
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  [self.scannerView addSubview:button];

  [NSLayoutConstraint activateConstraints:@[
    [button.centerXAnchor constraintEqualToAnchor:toolbar.centerXAnchor],
    [button.centerYAnchor constraintEqualToAnchor:toolbar.centerYAnchor],
  ]];

  [button addTarget:self
                action:@selector(didTapEnterManually:)
      forControlEvents:UIControlEventTouchUpInside];
}

- (void)didTapEnterManually:(id)sender {
  [self dismissForReason:scannerViewController::CLOSE_BUTTON
          withCompletion:nil];
}

@end
