// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_view_finder_coordinator.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_configuration_factory.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/model/lens_view_finder_metrics_recorder.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_view_finder_transition_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"

namespace {

// Maps the presentation style to transition type.
LensViewFinderTransition TransitionFromPresentationStyle(
    LensInputSelectionPresentationStyle style) {
  switch (style) {
    case LensInputSelectionPresentationStyle::SlideFromLeft:
      return LensViewFinderTransitionSlideFromLeft;
    case LensInputSelectionPresentationStyle::SlideFromRight:
      return LensViewFinderTransitionSlideFromRight;
  }
}

}  // namespace

@interface LensViewFinderCoordinator () <
    LensCommands,
    ChromeLensViewFinderDelegate,
    UIViewControllerTransitioningDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation LensViewFinderCoordinator {
  // The user interface to be presented.
  UIViewController<ChromeLensViewFinderController>* _lensViewController;

  // Manages the presenting & dismissal of the LVF user interface.
  LensViewFinderTransitionManager* _transitionManager;

  /// Forces the device orientation in portrait mode.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;

  /// Records LVF related metrics.
  LensViewFinderMetricsRecorder* _metricsRecorder;
}

@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBaseViewController:nil browser:browser];
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  _metricsRecorder = [[LensViewFinderMetricsRecorder alloc] init];
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensCommands)];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [self lockOrientationPortrait:NO];
  _metricsRecorder = nil;
  [super stop];
}

#pragma mark - LensCommands

- (void)searchImageWithLens:(SearchImageWithLensCommand*)command {
  id<LensOverlayCommands> _lensOverlayCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  [_lensOverlayCommands
      searchImageWithLens:command.image
               entrypoint:LensOverlayEntrypoint::kSearchImageContextMenu
               completion:nil];
}

- (void)openLensInputSelection:(OpenLensInputSelectionCommand*)command {
  LensOverlayConfigurationFactory* configurationFactory =
      [[LensOverlayConfigurationFactory alloc] init];
  LensConfiguration* configuration = [configurationFactory
      configurationForLensEntrypoint:command.entryPoint
                             profile:self.browser->GetProfile()];

  _transitionManager = [[LensViewFinderTransitionManager alloc]
      initWithLVFTransitionType:TransitionFromPresentationStyle(
                                    command.presentationStyle)];

  _lensViewController =
      ios::provider::NewChromeLensViewFinderController(configuration);
  [_lensViewController setLensViewFinderDelegate:self];

  _lensViewController.transitioningDelegate = _transitionManager;
  _lensViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  _lensViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;

  [_metricsRecorder recordLensViewFinderOpened];
  [self.baseViewController presentViewController:_lensViewController
                                        animated:YES
                                      completion:nil];
}

- (void)lensOverlayWillDismissWithCause:
    (LensOverlayDismissalCause)dismissalCause {
  // If it was a swipe down of the bottom sheet, restart capturing.
  if (dismissalCause == LensOverlayDismissalCauseSwipeDownFromSelection) {
    [_lensViewController buildCaptureInfrastructureForSelection];
  } else if (dismissalCause ==
             LensOverlayDismissalCauseSwipeDownFromTranslate) {
    [_lensViewController buildCaptureInfrastructureForTranslate];
  }
}

- (void)lensOverlayDidDismissWithCause:
    (LensOverlayDismissalCause)dismissalCause {
  if (dismissalCause != LensOverlayDismissalCauseSwipeDownFromSelection &&
      dismissalCause != LensOverlayDismissalCauseSwipeDownFromTranslate) {
    // All other dismissal sources cause the UI to shut down.
    [self exitLensViewFinderAnimated:NO];
  }
}

#pragma mark - ChromeLensViewFinderDelegate

- (void)lensController:(id<ChromeLensViewFinderController>)lensController
    didSelectImageWithMetadata:(id<LensImageMetadata>)imageMetadata {
  LensOverlayEntrypoint entrypoint =
      imageMetadata.isCameraImage ? LensOverlayEntrypoint::kLVFCameraCapture
                                  : LensOverlayEntrypoint::kLVFImagePicker;

  id<LensOverlayCommands> _lensOverlayCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  __weak id<ChromeLensViewFinderController> weakLensViewController =
      _lensViewController;

  // Once post capture is presented, the live camera can be torn down.
  [_lensOverlayCommands
      searchWithLensImageMetadata:imageMetadata
                       entrypoint:entrypoint
                       completion:^(BOOL success) {
                         [weakLensViewController tearDownCaptureInfrastructure];
                       }];
}

- (void)lensController:(id<ChromeLensViewFinderController>)lensController
          didSelectURL:(GURL)url {
  // NO-OP
}

- (void)lensControllerDidTapDismissButton:
    (id<ChromeLensViewFinderController>)lensController {
  [_metricsRecorder recordLensViewFinderDismissTapped];
  [self exitLensViewFinderAnimated:YES];
}

- (void)lensControllerWillAppear:
    (id<ChromeLensViewFinderController>)lensController {
  [self lockOrientationPortrait:YES];
}

- (void)lensControllerWillDisappear:
    (id<ChromeLensViewFinderController>)lensController {
  [self lockOrientationPortrait:NO];
}

#pragma mark - Private

- (void)exitLensViewFinderAnimated:(BOOL)animated {
  if (self.baseViewController.presentedViewController == _lensViewController) {
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:nil];
  }
}

- (void)lockOrientationPortrait:(BOOL)portraitLock {
  if (!portraitLock) {
    _scopedForceOrientation = nil;
    return;
  }

  SceneState* sceneState = self.browser->GetSceneState();
  if (!self.browser) {
    return;
  }
  if (AppState* appState = sceneState.profileState.appState) {
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
  }
}

@end
