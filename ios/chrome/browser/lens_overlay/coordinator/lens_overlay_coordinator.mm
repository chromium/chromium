// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@interface LensOverlayCoordinator () <LensOverlayCommands>

@end

@implementation LensOverlayCoordinator {
  /// Container view controller.
  /// Hosts all of lens UI: contains the selection UI, presents the results UI
  /// modally.
  LensOverlayContainerViewController* _containerViewController;
}

#pragma mark - properties

- (void)createContainerViewController {
  if (_containerViewController) {
    return;
  }
  _containerViewController = [[LensOverlayContainerViewController alloc] init];
  _containerViewController.modalPresentationStyle =
      UIModalPresentationOverFullScreen;
  _containerViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(base::FeatureList::IsEnabled(kEnableLensOverlay));
  [super start];

  Browser* browser = self.browser;
  DCHECK(browser);

  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensOverlayCommands)];
}

- (void)stop {
  Browser* browser = self.browser;
  DCHECK(browser);
  [browser->GetCommandDispatcher() stopDispatchingToTarget:self];

  [super stop];
}

#pragma mark - LensOverlayCommands

- (void)createAndShowLensUI:(BOOL)animated {
  if ([self isUICreated]) {
    // The UI is probably associated with the non-active tab. Destroy it with no
    // animation.
    [self destroyLensUI:NO];
  }
  [self createContainerViewController];
  [self showLensUI:animated];
}

- (void)showLensUI:(BOOL)animated {
  if (![self isUICreated]) {
    return;
  }

  [self.baseViewController presentViewController:_containerViewController
                                        animated:animated
                                      completion:nil];
}

- (void)hideLensUI:(BOOL)animated {
  if (![self isUICreated]) {
    return;
  }

  [_containerViewController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:nil];
}

- (void)destroyLensUI:(BOOL)animated {
  if (_containerViewController.presentingViewController) {
    [_containerViewController.presentingViewController
        dismissViewControllerAnimated:animated
                           completion:^{
                             [self destroyViewControllers];
                           }];
  } else {
    [self destroyViewControllers];
  }
}

#pragma mark - private

- (BOOL)isUICreated {
  return _containerViewController != nil;
}

// Disconnect and destroy all of the owned view controllers.
- (void)destroyViewControllers {
  _containerViewController = nil;
}

@end
