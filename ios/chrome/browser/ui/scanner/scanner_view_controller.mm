// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scanner/scanner_view_controller.h"

#import <AVFoundation/AVFoundation.h>

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/ui/commands/load_query_commands.h"
#include "ios/chrome/browser/ui/scanner/scanner_alerts.h"
#include "ios/chrome/browser/ui/scanner/scanner_presenting.h"
#include "ios/chrome/browser/ui/scanner/scanner_transitioning_delegate.h"
#include "ios/chrome/browser/ui/scanner/scanner_view.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@interface ScannerViewController ()

@property(nonatomic, readwrite, weak) id<LoadQueryCommands> queryLoader;

@end

@implementation ScannerViewController

#pragma mark - lifecycle

- (instancetype)initWithPresentationProvider:
    (id<ScannerPresenting>)presentationProvider {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _presentationProvider = presentationProvider;
  }
  return self;
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self dismissForReason:scannerViewController::CLOSE_BUTTON
          withCompletion:nil];
  return YES;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  DCHECK(self.cameraController);

  [self.view addSubview:self.scannerView];

  // Constraints for |self.scannerView|.
  [self.scannerView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [[self.scannerView leadingAnchor]
        constraintEqualToAnchor:[self.view leadingAnchor]],
    [[self.scannerView trailingAnchor]
        constraintEqualToAnchor:[self.view trailingAnchor]],
    [[self.scannerView topAnchor]
        constraintEqualToAnchor:[self.view topAnchor]],
    [[self.scannerView bottomAnchor]
        constraintEqualToAnchor:[self.view bottomAnchor]],
  ]];

  AVCaptureVideoPreviewLayer* previewLayer = [self.scannerView getPreviewLayer];

  switch ([self.cameraController getAuthorizationStatus]) {
    case AVAuthorizationStatusNotDetermined:
      [self.cameraController
          requestAuthorizationAndLoadCaptureSession:previewLayer];
      break;
    case AVAuthorizationStatusAuthorized:
      [self.cameraController loadCaptureSession:previewLayer];
      break;
    case AVAuthorizationStatusRestricted:
    case AVAuthorizationStatusDenied:
      // If this happens, then the user is really unlucky:
      // The authorization status changed in between the moment this VC was
      // instantiated and presented, and the moment viewDidLoad was called.
      [self dismissForReason:scannerViewController::
                                 IMPOSSIBLY_UNLIKELY_AUTHORIZATION_CHANGE
              withCompletion:nil];
      break;
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self startReceivingNotifications];
  [self.cameraController startRecording];

  // Reset torch.
  [self setTorchMode:AVCaptureTorchModeOff];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  CGFloat epsilon = 0.0001;
  // Note: targetTransform is always either identity or a 90, -90, or 180 degree
  // rotation.
  CGAffineTransform targetTransform = coordinator.targetTransform;
  CGFloat angle = atan2f(targetTransform.b, targetTransform.a);
  if (fabs(angle) > epsilon) {
    // Rotate the preview in the opposite direction of the interface rotation
    // and add a small value to the angle to force the rotation to occur in the
    // correct direction when rotating by 180 degrees.
    void (^animationBlock)(id<UIViewControllerTransitionCoordinatorContext>) =
        ^void(id<UIViewControllerTransitionCoordinatorContext> context) {
          [self.scannerView rotatePreviewByAngle:(epsilon - angle)];
        };
    // Note: The completion block is called even if the animation is
    // interrupted, for example by pressing the home button, with the same
    // target transform as the animation block.
    void (^completionBlock)(id<UIViewControllerTransitionCoordinatorContext>) =
        ^void(id<UIViewControllerTransitionCoordinatorContext> context) {
          [self.scannerView finishPreviewRotation];
        };
    [coordinator animateAlongsideTransition:animationBlock
                                 completion:completionBlock];
  } else if (!CGSizeEqualToSize(self.view.frame.size, size)) {
    // Reset the size of the preview if the bounds of the view controller
    // changed. This can happen if entering or leaving Split View mode on iPad.
    [self.scannerView resetPreviewFrame:size];
    [self.cameraController
        resetVideoOrientation:[self.scannerView getPreviewLayer]];
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self.cameraController stopRecording];
  [self stopReceivingNotifications];

  // Reset torch.
  [self setTorchMode:AVCaptureTorchModeOff];
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

#pragma mark - public methods

- (void)dismissForReason:(scannerViewController::DismissalReason)reason
          withCompletion:(void (^)(void))completion {
  [self.presentationProvider dismissScannerViewController:self
                                               completion:completion];
}

- (ScannerView*)buildScannerView {
  NOTIMPLEMENTED();
  return nil;
}

- (CameraController*)buildCameraController {
  NOTIMPLEMENTED();
  return nil;
}

#pragma mark - private methods

// Starts receiving notifications about the UIApplication going to background.
- (void)startReceivingNotifications {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleUIApplicationWillResignActiveNotification)
             name:UIApplicationWillResignActiveNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector
         (handleUIAccessibilityAnnouncementDidFinishNotification:)
             name:UIAccessibilityAnnouncementDidFinishNotification
           object:nil];
}

// Stops receiving all notifications.
- (void)stopReceivingNotifications {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

// Requests the torch mode to be set to |mode| by the |self.cameraController|
// and the icon of the torch button to be changed by the |self.scannerView|.
- (void)setTorchMode:(AVCaptureTorchMode)mode {
  [self.cameraController setTorchMode:mode];
}

- (ScannerView*)scannerView {
  if (!_scannerView) {
    _scannerView = [self buildScannerView];
  }
  return _scannerView;
}

- (CameraController*)cameraController {
  if (!_cameraController) {
    _cameraController = [self buildCameraController];
  }
  return _cameraController;
}

#pragma mark - notification handlers

- (void)handleUIApplicationWillResignActiveNotification {
  [self setTorchMode:AVCaptureTorchModeOff];
}

- (void)handleUIAccessibilityAnnouncementDidFinishNotification:
    (NSNotification*)notification {
  NSString* announcement = [[notification userInfo]
      valueForKey:UIAccessibilityAnnouncementKeyStringValue];
  if ([announcement
          isEqualToString:
              l10n_util::GetNSString(
                  IDS_IOS_SCANNER_SCANNED_ACCESSIBILITY_ANNOUNCEMENT)]) {
    DCHECK(_result);
    [self dismissForReason:scannerViewController::SCAN_COMPLETE
            withCompletion:^{
              [self.queryLoader loadQuery:_result
                              immediately:self.loadResultImmediately];
            }];
  }
}

#pragma mark - CameraControllerDelegate

- (void)captureSessionIsConnected {
  [self.cameraController setViewport:[self.scannerView viewportRectOfInterest]];
}

- (void)cameraStateChanged:(scanner::CameraState)state {
  switch (state) {
    case scanner::CAMERA_AVAILABLE:
      // Dismiss any presented alerts.
      if ([self presentedViewController]) {
        [self dismissViewControllerAnimated:YES completion:nil];
      }
      break;
    case scanner::CAMERA_IN_USE_BY_ANOTHER_APPLICATION:
    case scanner::MULTIPLE_FOREGROUND_APPS:
    case scanner::CAMERA_PERMISSION_DENIED:
    case scanner::CAMERA_UNAVAILABLE_DUE_TO_SYSTEM_PRESSURE:
    case scanner::CAMERA_UNAVAILABLE: {
      // Dismiss any presented alerts.
      if ([self presentedViewController]) {
        [self dismissViewControllerAnimated:YES completion:nil];
      }
      [self presentViewController:
                scanner::DialogForCameraState(
                    state,
                    ^(UIAlertAction*) {
                      [self dismissForReason:scannerViewController::ERROR_DIALOG
                              withCompletion:nil];
                    })
                         animated:YES
                       completion:nil];
      break;
    }
    case scanner::CAMERA_NOT_LOADED:
      NOTREACHED();
      break;
  }
}

- (void)torchStateChanged:(BOOL)torchIsOn {
  [self.scannerView setTorchButtonTo:torchIsOn];
}

- (void)torchAvailabilityChanged:(BOOL)torchIsAvailable {
  [self.scannerView enableTorchButton:torchIsAvailable];
}

#pragma mark - ScannerViewDelegate

- (void)dismissScannerView:(id)sender {
  [self dismissForReason:scannerViewController::CLOSE_BUTTON
          withCompletion:nil];
}

- (void)toggleTorch:(id)sender {
  if ([self.cameraController isTorchActive]) {
    [self setTorchMode:AVCaptureTorchModeOff];
  } else {
    base::RecordAction(UserMetricsAction("MobileQRScannerTorchOn"));
    [self setTorchMode:AVCaptureTorchModeOn];
  }
}

@end
