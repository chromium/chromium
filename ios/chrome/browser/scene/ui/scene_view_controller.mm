// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/ui/scene_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/scene/ui/scene_view.h"
#import "ios/chrome/browser/scene/ui/scene_view_delegate.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface SceneViewController () <SceneViewDelegate>
@end

@implementation SceneViewController {
  // The app bar.
  UIViewController* _appBar;
  // The view containing the app (the part outside the app bar).
  UIView* _appContentView;
  NSArray<NSLayoutConstraint*>* _portraitConstraints;
  NSArray<NSLayoutConstraint*>* _landscapeLeftConstraints;
  NSArray<NSLayoutConstraint*>* _landscapeRightConstraints;
  // The last fullscreen progress value received.
  CGFloat _fullscreenProgress;
}

#pragma mark - UIViewController

- (void)loadView {
  SceneView* view = [[SceneView alloc] init];
  view.delegate = self;
  if (!IsFullscreenRefactoringEnabled()) {
    view.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  self.view = view;
  _fullscreenProgress = 1;
}

- (void)viewDidLoad {
  CHECK(self.layoutGuideCenter);
  [super viewDidLoad];
  UIView* view = self.view;
  _appContentView = [[UIView alloc] init];
  if (IsFullscreenRefactoringEnabled()) {
    _appContentView.translatesAutoresizingMaskIntoConstraints = NO;
  } else {
    _appContentView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  [view addSubview:_appContentView];
  _appContentView.frame = view.bounds;
  [self.layoutGuideCenter referenceView:_appContentView
                              underName:kAppContentGuide];
  if (!IsChromeNextIaEnabled()) {
    AddSameConstraints(_appContentView, view);
  }
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf updateLayoutForAppBar];
      }
                      completion:nil];
}

#pragma mark - Public

- (UIView*)appContainer {
  [self loadViewIfNeeded];
  return _appContentView;
}

- (void)setAppBar:(UIViewController*)appBar {
  CHECK(!_appBar);
  [self loadViewIfNeeded];
  _appBar = appBar;
  UIView* appBarView = appBar.view;
  appBarView.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* view = self.view;

  [self addChildViewController:appBar];
  [view addSubview:appBarView];

  AddSameCenterConstraints(view, appBarView);

  [appBar didMoveToParentViewController:self];

  UIView* appBarRealView =
      [self.layoutGuideCenter referencedViewUnderName:kAppBarGuide];

  if (!IsFullscreenRefactoringEnabled()) {
    [self updateLayoutForAppBar];
    return;
  }

  _portraitConstraints = @[
    [_appContentView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_appContentView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [_appContentView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],
    [_appContentView.bottomAnchor
        constraintEqualToAnchor:appBarRealView.topAnchor],
  ];
  _landscapeLeftConstraints = @[
    [_appContentView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_appContentView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor
                                                  constant:kAppBarHeight],
    [_appContentView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],
    [_appContentView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
  ];
  _landscapeRightConstraints = @[
    [_appContentView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_appContentView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [_appContentView.trailingAnchor constraintEqualToAnchor:view.trailingAnchor
                                                   constant:-kAppBarHeight],
    [_appContentView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
  ];

  [self updateLayoutForAppBar];
}

#pragma mark - SceneViewDelegate

- (void)sceneViewDidMoveToWindow:(SceneView*)sceneView {
  [self updateLayoutForAppBar];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  UIInterfaceOrientation orientation = [self currentOrientation];
  if (orientation != UIInterfaceOrientationPortrait) {
    return;
  }
  _fullscreenProgress = progress;
  [self updateLayoutForAppBar];
}

#pragma mark - Private

// Returns the current orientation of the scene.
- (UIInterfaceOrientation)currentOrientation {
  return self.view.window.windowScene.effectiveGeometry.interfaceOrientation;
}

// Updates the layout to adapt to screen changes.
- (void)updateLayoutForAppBar {
  UIWindowScene* windowScene = self.view.window.windowScene;
  if (!windowScene || !_appBar) {
    return;
  }

  UIInterfaceOrientation orientation = [self currentOrientation];

  if (!IsFullscreenRefactoringEnabled()) {
    CGRect frame = self.view.bounds;
    UIEdgeInsets insets = UIEdgeInsetsZero;
    switch (orientation) {
      case UIInterfaceOrientationLandscapeLeft:
        insets = UIEdgeInsetsMake(0, kAppBarHeight, 0, 0);
        break;

      case UIInterfaceOrientationLandscapeRight:
        insets = UIEdgeInsetsMake(0, 0, 0, kAppBarHeight);
        break;

      case UIInterfaceOrientationPortrait: {
        CGFloat appBarHeight =
            kAppBarHeightFullscreen -
            _fullscreenProgress * (kAppBarHeightFullscreen - kAppBarHeight);
        insets = UIEdgeInsetsMake(0, 0, appBarHeight, 0);
        break;
      }

      default:
        break;
    }
    _appContentView.frame = UIEdgeInsetsInsetRect(frame, insets);
    return;
  }

  [NSLayoutConstraint deactivateConstraints:_portraitConstraints];
  [NSLayoutConstraint deactivateConstraints:_landscapeLeftConstraints];
  [NSLayoutConstraint deactivateConstraints:_landscapeRightConstraints];

  switch (orientation) {
    case UIInterfaceOrientationLandscapeLeft:
      [NSLayoutConstraint activateConstraints:_landscapeLeftConstraints];
      break;

    case UIInterfaceOrientationLandscapeRight:
      [NSLayoutConstraint activateConstraints:_landscapeRightConstraints];
      break;

    case UIInterfaceOrientationPortrait:
      [NSLayoutConstraint activateConstraints:_portraitConstraints];
      break;

    default:
      break;
  }

  [self.view layoutIfNeeded];
}

@end
