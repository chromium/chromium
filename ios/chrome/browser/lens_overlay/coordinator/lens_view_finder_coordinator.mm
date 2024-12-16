// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_view_finder_coordinator.h"

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_configuration_factory.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"

@interface LensViewFinderCoordinator () <LensCommands,
                                         ChromeLensControllerDelegate>
@end

@implementation LensViewFinderCoordinator {
  // Controls the lens view finder experience.
  id<ChromeLensController> _lensController;

  // The user interface to be presented.
  __weak UIViewController* _lensViewController;
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
  _lensController = ios::provider::NewChromeLensController(configuration);
  _lensController.delegate = self;

  _lensViewController = _lensController.inputSelectionViewController;
  _lensViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  _lensViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  [self.baseViewController presentViewController:_lensViewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - ChromeLensControllerDelegate

- (void)lensControllerDidGenerateImage:(UIImage*)image {
}

- (void)lensControllerDidGenerateLoadParams:
    (const web::NavigationManager::WebLoadParams&)params {
}

- (void)lensControllerDidSelectURL:(NSURL*)url {
}

- (void)lensControllerDidTapDismissButton {
  if (self.baseViewController.presentedViewController == _lensViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }
}

- (CGRect)webContentFrame {
  return [UIScreen mainScreen].bounds;
}

@end
