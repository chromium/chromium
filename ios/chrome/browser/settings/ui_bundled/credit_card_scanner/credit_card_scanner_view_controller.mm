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
  [self setupEnterManuallyButton];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Crop the scanner subviews to the size of the `scannerView` to avoid the
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

  // The toolbar is expected to have exactly 3 items by default, set up in the
  // base class `ScannerView` (represented as `@[ close, spacer, _torchButton
  // ]`).
  if (!toolbar || toolbar.items.count != 3) {
    return;
  }

  // Create a native UIBarButtonItem. On iOS 26, plain bar items on transparent
  // toolbars automatically get the premium native glass pill style.
  UIBarButtonItem* enterManuallyItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_AUTOFILL_SCAN_CARD_ENTER_MANUALLY)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(didTapEnterManually:)];

  NSMutableArray<UIBarButtonItem*>* items = [toolbar.items mutableCopy];
  [items insertObject:enterManuallyItem atIndex:2];
  [items insertObject:
             [[UIBarButtonItem alloc]
                 initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                                      target:nil
                                      action:nil]
              atIndex:3];
  toolbar.items = items;
}

- (void)didTapEnterManually:(id)sender {
  [self dismissForReason:scannerViewController::CLOSE_BUTTON
          withCompletion:nil];
}

@end
