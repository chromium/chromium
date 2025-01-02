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

// The corner radius to be applied on the bottom of the image passed to post
// capture
const CGFloat kBottomCornerRadius = 108.0;

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

  [self.baseViewController presentViewController:_lensViewController
                                        animated:YES
                                      completion:nil];
}

- (void)lensOverlayDismissed {
  [self exitLensViewFinder];
}

#pragma mark - ChromeLensViewFinderDelegate

- (void)lensController:(id<ChromeLensViewFinderController>)lensController
             didSelectImage:(UIImage*)image
    serializedViewportState:(NSString*)viewportState
              isCameraImage:(BOOL)isCameraImage {
  BOOL isPortrait = image.size.height > image.size.width;
  if (isCameraImage && isPortrait) {
    image = [self infilledImageForPortraitCameraCapture:image];
  }

  LensOverlayEntrypoint entrypoint =
      isCameraImage ? LensOverlayEntrypoint::kLVFCameraCapture
                    : LensOverlayEntrypoint::kLVFImagePicker;

  id<LensOverlayCommands> _lensOverlayCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), LensOverlayCommands);
  __weak id<ChromeLensViewFinderController> weakLensViewController =
      _lensViewController;

  // Once post capture is presented, the live camera can be torn down.
  [_lensOverlayCommands
      searchImageWithLens:image
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
  [self exitLensViewFinder];
}

#pragma mark - Private
- (void)exitLensViewFinder {
  if (self.baseViewController.presentedViewController == _lensViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }
}

// Rounds the bottom corners of the image and pads the bottom edge to match the
// size of the viewport.
- (UIImage*)infilledImageForPortraitCameraCapture:(UIImage*)image {
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = 1;

  CGSize screenSize = [UIScreen mainScreen].bounds.size;
  CGFloat scale = 3;
  CGSize scaledScreenSize =
      CGSizeMake(screenSize.width * scale, screenSize.height * scale);

  CGFloat originalAspectRatio = image.size.width / image.size.height;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:scaledScreenSize
                                             format:format];

  CGRect imageDrawRect =
      CGRectMake(0, 0, scaledScreenSize.width,
                 scaledScreenSize.width / originalAspectRatio);

  UIBezierPath* path = [UIBezierPath
      bezierPathWithRoundedRect:imageDrawRect
              byRoundingCorners:UIRectCornerBottomLeft | UIRectCornerBottomRight
                    cornerRadii:CGSizeMake(kBottomCornerRadius,
                                           kBottomCornerRadius)];

  UIImage* imageWithInfill =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [[UIColor whiteColor] setFill];
        UIRectFill(context.format.bounds);
        [path addClip];
        [image drawInRect:imageDrawRect];
      }];

  return imageWithInfill;
}

@end
