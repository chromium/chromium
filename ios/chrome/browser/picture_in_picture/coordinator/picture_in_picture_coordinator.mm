// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/picture_in_picture/coordinator/picture_in_picture_coordinator.h"

#import <AVKit/AVKit.h>

#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/browser/picture_in_picture/coordinator/picture_in_picture_mediator.h"
#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_configuration.h"
#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/picture_in_picture_commands.h"

@interface PictureInPictureCoordinator () <
    UIAdaptivePresentationControllerDelegate>
@end

@implementation PictureInPictureCoordinator {
  PictureInPictureViewController* _viewController;
  UINavigationController* _navigationController;
  PictureInPictureMediator* _mediator;
  PictureInPictureConfiguration* _configuration;
  id<PictureInPictureCommands> _handler;
}

- (instancetype)initWithConfiguration:
                    (PictureInPictureConfiguration*)configuration
                   baseViewController:(UIViewController*)viewController
                              browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _configuration = configuration;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (![AVPictureInPictureController isPictureInPictureSupported]) {
    // Picture in picture is not supported, open the feature's destination
    // directly.
    [self openFeatureDestination];
    [self recordDismissalReason:PictureInPictureDismissalReason::kNotSupported];
    [_handler dismissPictureInPicture];
    return;
  }

  _mediator =
      [[PictureInPictureMediator alloc] initWithConfiguration:_configuration];
  _viewController = [[PictureInPictureViewController alloc]
           initWithTitle:_configuration.title
      primaryButtonTitle:_configuration.primaryButtonTitle
                videoURL:_configuration.videoURL
                 feature:_configuration.feature];

  _handler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                PictureInPictureCommands);
  _viewController.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismiss)];
  _viewController.actionDelegate = _mediator;
  _viewController.mutator = _mediator;
  _viewController.handler = _handler;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.presentationController.delegate = self;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_mediator recordPrimaryButtonTapCount];
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _viewController = nil;
  _mediator = nil;
  _navigationController = nil;
}

#pragma mark - Public

- (void)dismissIfNotPipRestore {
  [_viewController dismissIfNotPipRestore];
}

#pragma mark - Private

// Dismisses the picture-in-picture view controller.
- (void)dismiss {
  [self
      recordDismissalReason:PictureInPictureDismissalReason::kInAppCloseButton];
  [_handler dismissPictureInPicture];
}

// Opens the feature's destination.
- (void)openFeatureDestination {
  switch (_configuration.feature) {
    case PictureInPictureFeature::kDefaultBrowser:
      OpenIOSDefaultBrowserSettingsPage(IsDefaultAppsPictureInPictureVariant());
      break;
  }
}

- (void)recordDismissalReason:(PictureInPictureDismissalReason)dismissalReason {
  base::UmaHistogramEnumeration(
      base::StrCat({"IOS.PictureInPicture.",
                    PictureInPictureFeatureToString(_configuration.feature),
                    ".DismissalReason"}),
      dismissalReason);
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self recordDismissalReason:PictureInPictureDismissalReason::
                                  kInAppSwipeToClose];
  [_handler dismissPictureInPicture];
}

@end
