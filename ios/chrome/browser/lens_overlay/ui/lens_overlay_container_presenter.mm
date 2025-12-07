// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_presenter.h"

#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/device_orientation/ui_bundled/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The duration of the animation when exiting the selection UI.
const CGFloat kSelectionViewExitAnimationDuration = 0.2f;

// The duration of the animation when changing the opacity of the selection UI.
const CGFloat kSelectionViewOpacityAnimationDuration = 0.4f;

}  // namespace

@interface LensOverlayContainerPresenter () <LensOverlayContainerDelegate>

@end

@implementation LensOverlayContainerPresenter {
  // The controller on which to present the container.
  __weak UIViewController* _baseViewController;

  // The container to be presented.
  __weak LensOverlayContainerViewController* _containerViewController;

  /// Forces the device orientation in portrait mode.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;

  // The top constraint for the controller.
  NSLayoutConstraint* _topConstraint;

  // Block to be called when the container is added to a view hierarchy.
  ProceduralBlock _callWhenContainerAppear;
}

- (BOOL)isLensOverlayVisible {
  return _containerViewController.isViewLoaded &&
         _containerViewController.view.window != nil;
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
                      completion:(void (^)(void))completion {
  if (!_baseViewController || !_containerViewController) {
    if (completion) {
      completion();
    }
    return;
  }

  _callWhenContainerAppear = completion;

  _containerViewController.delegate = self;
  AppState* appState = sceneState.profileState.appState;
  ProfileIOS* profile = sceneState.profileState.profile;
  CHECK(profile, kLensOverlayNotFatalUntil);
  if (appState && profile &&
      !IsLensOverlayLandscapeOrientationEnabled(profile->GetPrefs())) {
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
  }

  [self.delegate lensOverlayContainerPresenterWillBeginPresentation:self];

  [_baseViewController.view endEditing:YES];

  [_containerViewController willMoveToParentViewController:_baseViewController];
  [_baseViewController addChildViewController:_containerViewController];
  [_baseViewController.view addSubview:_containerViewController.view];
  [_containerViewController didMoveToParentViewController:_baseViewController];

  _containerViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  NSDirectionalEdgeInsets insets =
      [self.delegate lensOverlayContainerPresenterInsetsForPresentation:self];

  AddSameConstraintsToSides(
      _containerViewController.view, _baseViewController.view,
      LayoutSides::kLeading | LayoutSides::kBottom | LayoutSides::kTrailing);
  _topConstraint = [_containerViewController.view.topAnchor
      constraintEqualToAnchor:_baseViewController.view.topAnchor
                     constant:insets.top];
  [NSLayoutConstraint activateConstraints:@[ _topConstraint ]];

  _containerViewController.selectionViewController.view.alpha = 1;

  if (!animated) {
    return;
  }

  _containerViewController.view.alpha = 0;
  __weak UIViewController* weakContainer = _containerViewController;
  [UIView animateWithDuration:kSelectionViewExitAnimationDuration
                   animations:^{
                     weakContainer.view.alpha = 1.0;
                   }
                   completion:nil];
}

- (void)dismissContainerAnimated:(BOOL)animated
                      completion:(void (^)())completion {
  _scopedForceOrientation.reset();
  _containerViewController.delegate = nil;
  [self.delegate lensOverlayContainerPresenterWillDismissPresentation:self];
  // If the container is not attached, directly call completion.
  if (!_containerViewController.view.superview) {
    if (completion) {
      completion();
    }
    return;
  }

  __weak UIViewController* weakContainer = _containerViewController;
  auto executeCleanup = ^{
    [weakContainer.view removeFromSuperview];
    [weakContainer removeFromParentViewController];
    if (completion) {
      completion();
    }
  };

  if (!animated) {
    executeCleanup();
    return;
  }

  [self fadeSelectionUIWithCompletion:executeCleanup];
}

- (void)fadeSelectionUIWithCompletion:(void (^)())completion {
  [_containerViewController
      fadeSelectionUIWithDuration:kSelectionViewExitAnimationDuration
                       completion:completion];
}

- (void)setContainerHidden:(BOOL)hidden animated:(BOOL)animated {
  CGFloat alpha = hidden ? 0 : 1;

  if (!animated) {
    _containerViewController.view.hidden = hidden;
    _containerViewController.view.alpha = alpha;
    return;
  }

  if (hidden && _containerViewController.view.hidden) {
    return;
  }

  _containerViewController.view.hidden = NO;

  __weak __typeof(_containerViewController) weakContainerVC =
      _containerViewController;
  [UIView animateWithDuration:kSelectionViewOpacityAnimationDuration
      animations:^{
        weakContainerVC.view.alpha = alpha;
      }
      completion:^(BOOL) {
        weakContainerVC.view.hidden = hidden;
      }];
}

#pragma mark - LensOverlayContainerDelegate

- (void)lensOverlayContainerDidAppear:(LensOverlayContainerViewController*)
                                          lensOverlayContainerViewController
                             animated:(BOOL)animated {
  if (_callWhenContainerAppear) {
    _callWhenContainerAppear();
    _callWhenContainerAppear = nil;
  }
  [self.delegate lensOverlayContainerPresenterDidCompletePresentation:self
                                                             animated:animated];
}

- (void)lensOverlayContainerDidChangeSizeClass:
    (LensOverlayContainerViewController*)lensOverlayContainerViewController {
  NSDirectionalEdgeInsets insets =
      [self.delegate lensOverlayContainerPresenterInsetsForPresentation:self];
  _topConstraint.constant = insets.top;
  [self.delegate lensOverlayContainerPresenterDidReadjustPresentation:self];
}

@end
