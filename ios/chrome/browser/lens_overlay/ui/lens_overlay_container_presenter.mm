// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_presenter.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"

namespace {

// The duration of the dismiss animation when exiting the selection UI.
const CGFloat kSelectionViewDismissAnimationDuration = 0.2f;

}  // namespace

@implementation LensOverlayContainerPresenter {
  // The controller on which to present the container.
  __weak UIViewController* _baseViewController;

  // The container to be presented.
  __weak LensOverlayContainerViewController* _containerViewController;

  /// Forces the device orientation in portrait mode.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;
}

- (BOOL)isLensOverlayVisible {
  return _containerViewController.presentingViewController != nil;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                   containerViewController:(LensOverlayContainerViewController*)
                                               containerViewController {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _containerViewController = containerViewController;
  }

  return self;
}

- (void)presentContainerAnimated:(BOOL)animated
                      sceneState:(SceneState*)sceneState
                      completion:(void (^)())completion {
  if (!_baseViewController || !_containerViewController) {
    if (completion) {
      completion();
    }
    return;
  }

  AppState* appState = sceneState.profileState.appState;
  if (appState) {
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
  }

  _containerViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  _containerViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;

  [_baseViewController presentViewController:_containerViewController
                                    animated:animated
                                  completion:completion];
}

- (void)dismissContainerAnimated:(BOOL)animated
                      completion:(void (^)())completion {
  if (!_containerViewController.presentingViewController) {
    if (completion) {
      _scopedForceOrientation.reset();
      completion();
    }
    return;
  }

  _scopedForceOrientation.reset();
  [_containerViewController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:completion];
}

- (void)fadeSelectionUIWithCompletion:(void (^)())completion {
  __weak UIViewController* weakContainer = _containerViewController;

  [UIView animateWithDuration:kSelectionViewDismissAnimationDuration
      animations:^{
        weakContainer.view.alpha = 0;
      }
      completion:^(BOOL success) {
        if (completion) {
          completion();
        }
      }];
}

@end
