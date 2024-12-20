// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_view_finder_coordinator.h"

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_configuration_factory.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_view_finder_transition_manager.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
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

@interface LensViewFinderCoordinator () <LensCommands,
                                         ChromeLensViewFinderDelegate>
@end

@implementation LensViewFinderCoordinator {
  // Controls the lens view finder experience.
  id<ChromeLensController> _lensController;

  // The user interface to be presented.
  UIViewController<ChromeLensViewFinderController>* _lensViewController;

  // Manages the presenting & dismissal of the LVF user interface.
  LensViewFinderTransitionManager* _transitionManager;
}

@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBaseViewController:nil browser:browser];
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensCommands)];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [super stop];
}

#pragma mark - LensCommands

- (void)searchImageWithLens:(SearchImageWithLensCommand*)command {
  id<LensOverlayCommands> _lensOverlayCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  [_lensOverlayCommands
      searchImageWithLens:command.image
               entrypoint:LensOverlayEntrypoint::kSearchImageContextMenu];
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
  [self.baseViewController presentViewController:_lensViewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - ChromeLensViewFinderDelegate

- (void)lensController:(id<ChromeLensViewFinderController>)lensController
             didSelectImage:(UIImage*)image
    serializedViewportState:(NSString*)viewportState
              isCameraImage:(BOOL)isCameraImage {
}

- (void)lensController:(id<ChromeLensViewFinderController>)lensController
          didSelectURL:(GURL)url {
}

- (void)lensControllerDidTapDismissButton:
    (id<ChromeLensViewFinderController>)lensController {
  if (self.baseViewController.presentedViewController == _lensViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }
}

@end
