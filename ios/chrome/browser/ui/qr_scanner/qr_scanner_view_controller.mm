// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_view_controller.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_camera_controller.h"
#include "ios/chrome/browser/ui/qr_scanner/qr_scanner_view.h"
#include "ios/chrome/browser/ui/scanner/scanner_alerts.h"
#include "ios/chrome/browser/ui/scanner/scanner_presenting.h"
#include "ios/chrome/browser/ui/scanner/scanner_transitioning_delegate.h"
#include "ios/chrome/browser/ui/scanner/scanner_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@interface QRScannerViewController () {
  // The transitioning delegate used for presenting and dismissing the QR
  // scanner.
  ScannerTransitioningDelegate* _transitioningDelegate;
}

@property(nonatomic, readwrite, weak) id<LoadQueryCommands> queryLoader;

// Whether VoiceOver detection has been overridden.
@property(nonatomic, assign) BOOL voiceOverCheckOverridden;

@end

@implementation QRScannerViewController

#pragma mark - Lifecycle

- (instancetype)
    initWithPresentationProvider:(id<ScannerPresenting>)presentationProvider
                     queryLoader:(id<LoadQueryCommands>)queryLoader {
  self = [super initWithPresentationProvider:presentationProvider];
  if (self) {
    _queryLoader = queryLoader;
  }
  return self;
}

#pragma mark - ScannerViewController

- (ScannerView*)buildScannerView {
  return [[QRScannerView alloc] initWithFrame:self.view.frame delegate:self];
}

- (CameraController*)buildCameraController {
  return [[QRScannerCameraController alloc] initWithQRScannerDelegate:self];
}

- (void)dismissForReason:(scannerViewController::DismissalReason)reason
          withCompletion:(void (^)(void))completion {
  switch (reason) {
    case scannerViewController::CLOSE_BUTTON:
      base::RecordAction(UserMetricsAction("MobileQRScannerClose"));
      break;
    case scannerViewController::ERROR_DIALOG:
      base::RecordAction(UserMetricsAction("MobileQRScannerError"));
      break;
    case scannerViewController::SCAN_COMPLETE:
      base::RecordAction(UserMetricsAction("MobileQRScannerScannedCode"));
      break;
    case scannerViewController::IMPOSSIBLY_UNLIKELY_AUTHORIZATION_CHANGE:
      break;
  }

  [super dismissForReason:reason withCompletion:completion];
}

#pragma mark - QRScannerCameraControllerDelegate

- (void)receiveQRScannerResult:(NSString*)result loadImmediately:(BOOL)load {
  if ([self isVoiceOverActive]) {
    // Post a notification announcing that a code was scanned. QR scanner will
    // be dismissed when the UIAccessibilityAnnouncementDidFinishNotification is
    // received.
    self.result = [result copy];
    self.loadResultImmediately = load;
    UIAccessibilityPostNotification(
        UIAccessibilityAnnouncementNotification,
        l10n_util::GetNSString(
            IDS_IOS_SCANNER_SCANNED_ACCESSIBILITY_ANNOUNCEMENT));
  } else {
    [self.scannerView animateScanningResultWithCompletion:^void(void) {
      [self dismissForReason:scannerViewController::SCAN_COMPLETE
              withCompletion:^{
                [self.queryLoader loadQuery:result immediately:load];
              }];
    }];
  }
}

#pragma mark - Public methods

- (UIViewController*)getViewControllerToPresent {
  DCHECK(self.cameraController);
  switch ([self.cameraController getAuthorizationStatus]) {
    case AVAuthorizationStatusNotDetermined:
    case AVAuthorizationStatusAuthorized:
      _transitioningDelegate = [[ScannerTransitioningDelegate alloc] init];
      [self setTransitioningDelegate:_transitioningDelegate];
      return self;
    case AVAuthorizationStatusRestricted:
    case AVAuthorizationStatusDenied:
      return scanner::DialogForCameraState(scanner::CAMERA_PERMISSION_DENIED,
                                           nil);
  }
}

#pragma mark - Private

// Returns whether voice over is active.
- (BOOL)isVoiceOverActive {
  return UIAccessibilityIsVoiceOverRunning() || self.voiceOverCheckOverridden;
}

#pragma mark - Testing Additions

- (void)overrideVoiceOverCheck:(BOOL)overrideVoiceOverCheck {
  self.voiceOverCheckOverridden = overrideVoiceOverCheck;
}

@end
