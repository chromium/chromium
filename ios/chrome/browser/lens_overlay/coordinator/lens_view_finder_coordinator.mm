// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_view_finder_coordinator.h"

#import "base/ios/block_types.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/device_orientation/ui_bundled/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_configuration_factory.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/model/lens_view_finder_metrics_recorder.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_view_finder_transition_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
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

// Whether post capture view is shown.
@property(nonatomic, assign) BOOL postCaptureShown;

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
  id<LensOverlayCommands> lensOverlayHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  [lensOverlayHandler
          searchImageWithLens:command.image
                   entrypoint:LensOverlayEntrypoint::kSearchImageContextMenu
      initialPresentationBase:_baseViewController
      resultsPresenterFactory:nil
                   completion:nil];
}

- (void)openLensInputSelection:(OpenLensInputSelectionCommand*)command {
  __weak __typeof(self) weakSelf = self;
  // As a new Lens sessions starts, cleanup any inactive post capture before
  // presenting the input selection UI.
  [self destroyInactivePostCaptureSessionsWithCompletion:^{
    [weakSelf presentLensInputSelectionUIForCommand:command];
  }];
}

- (void)lensOverlayWillDismissWithCause:
    (LensOverlayDismissalCause)dismissalCause {
  if (!self.postCaptureShown) {
    return;
  }

  // If it was a swipe down of the bottom sheet, restart capturing.
  if (dismissalCause == LensOverlayDismissalCauseSwipeDownFromSelection) {
    [_lensViewController buildCaptureInfrastructureForSelection];
  } else if (dismissalCause ==
             LensOverlayDismissalCauseSwipeDownFromTranslate) {
    [_lensViewController buildCaptureInfrastructureForTranslate];
  } else if (dismissalCause == LensOverlayDismissalCauseDismissButton) {
    [_lensViewController tearDownCaptureInfrastructureWithPlaceholder:NO];
  }
}

- (void)lensOverlayDidDismissWithCause:
    (LensOverlayDismissalCause)dismissalCause {
  if (!self.postCaptureShown) {
    return;
  }

  self.postCaptureShown = NO;
  if (dismissalCause != LensOverlayDismissalCauseSwipeDownFromSelection &&
      dismissalCause != LensOverlayDismissalCauseSwipeDownFromTranslate) {
    // All other dismissal sources cause the UI to shut down.
    [self exitLensViewFinderAnimated:NO completion:nil];
  }
}

#pragma mark - ChromeLensViewFinderDelegate

- (void)lensController:(id<ChromeLensViewFinderController>)lensController
    didSelectImageWithMetadata:(id<LensImageMetadata>)imageMetadata {
  [_lensViewController tearDownCaptureInfrastructureWithPlaceholder:YES];

  __weak __typeof(self) weakSelf = self;
  auto startPostCapture = ^{
    [weakSelf startPostCaptureWithMetadata:imageMetadata];
  };
  if (_lensViewController.presentedViewController) {
    [_lensViewController.presentedViewController
        dismissViewControllerAnimated:YES
                           completion:startPostCapture];
  } else {
    startPostCapture();
  }
}

- (void)lensController:(id<ChromeLensViewFinderController>)lensController
          didSelectURL:(GURL)url {
  [_metricsRecorder recordLensViewFinderCameraURLOpen];
  __weak __typeof(self) weakSelf = self;
  [self exitLensViewFinderAnimated:YES
                        completion:^{
                          [weakSelf openInNewTab:url];
                        }];
}

- (void)lensControllerDidTapDismissButton:
    (id<ChromeLensViewFinderController>)lensController {
  [_metricsRecorder recordLensViewFinderDismissTapped];
  [self exitLensViewFinderAnimated:YES completion:nil];
}

- (void)lensControllerWillAppear:
    (id<ChromeLensViewFinderController>)lensController {
  [self lockOrientationPortrait:YES];
}

- (void)lensControllerWillDisappear:
    (id<ChromeLensViewFinderController>)lensController {
  [self lockOrientationPortrait:NO];
  self.postCaptureShown = NO;
}

#pragma mark - Private

- (void)presentLensInputSelectionUIForCommand:
    (OpenLensInputSelectionCommand*)command {
  [self cancelOmniboxEdit];
  [self prepareLensViewControllerForCommand:command];

  if (!_lensViewController) {
    return;
  }

  [_lensViewController setLensViewFinderDelegate:self];
  [_metricsRecorder recordLensViewFinderOpened];
  [self.baseViewController
      presentViewController:_lensViewController
                   animated:YES
                 completion:command.presentationCompletion];
}

- (void)prepareLensViewControllerForCommand:
    (OpenLensInputSelectionCommand*)command {
  LensOverlayConfigurationFactory* configurationFactory =
      [[LensOverlayConfigurationFactory alloc] init];
  LensConfiguration* configuration =
      [configurationFactory configurationForLensEntrypoint:command.entryPoint
                                                   profile:self.profile];

  _transitionManager = [[LensViewFinderTransitionManager alloc]
      initWithLVFTransitionType:TransitionFromPresentationStyle(
                                    command.presentationStyle)];

  _lensViewController =
      ios::provider::NewChromeLensViewFinderController(configuration);

  _lensViewController.transitioningDelegate = _transitionManager;
  _lensViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  _lensViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;

  [_lensViewController setLensViewFinderDelegate:nil];
}

- (void)destroyInactivePostCaptureSessionsWithCompletion:
    (ProceduralBlock)completion {
  id<LensOverlayCommands> lensOverlayCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  [lensOverlayCommands
      destroyLensUI:NO
             reason:lens::LensOverlayDismissalSource::kSearchWithCameraRequested
         completion:completion];
}

- (void)exitLensViewFinderAnimated:(BOOL)animated
                        completion:(ProceduralBlock)completion {
  if (_lensViewController &&
      self.baseViewController.presentedViewController == _lensViewController) {
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:completion];
  } else if (completion) {
    completion();
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

- (void)openInNewTab:(GURL)URL {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL
                                      inIncognito:self.isOffTheRecord];

  [HandlerForProtocol(self.browser->GetCommandDispatcher(), ApplicationCommands)
      openURLInNewTab:command];
}

- (void)startPostCaptureWithMetadata:(id<LensImageMetadata>)imageMetadata {
  LensOverlayEntrypoint entrypoint =
      imageMetadata.isCameraImage ? LensOverlayEntrypoint::kLVFCameraCapture
                                  : LensOverlayEntrypoint::kLVFImagePicker;

  __weak __typeof(self) weakSelf = self;
  id<LensOverlayCommands> lensOverlayCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  [lensOverlayCommands searchWithLensImageMetadata:imageMetadata
                                        entrypoint:entrypoint
                           initialPresentationBase:_lensViewController
                                        completion:^(BOOL) {
                                          weakSelf.postCaptureShown = YES;
                                        }];
}

// Cancel any editing before presenting the Lens View Finder experience to
// prevent the omnibox popup from obscuring the view.
- (void)cancelOmniboxEdit {
  Browser* browser = self.browser;
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<OmniboxCommands> omniboxCommandsHandler =
      HandlerForProtocol(dispatcher, OmniboxCommands);
  [omniboxCommandsHandler cancelOmniboxEdit];
}

@end
