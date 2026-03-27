// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/ui/scene_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_utils.h"
#import "ios/chrome/browser/scene/ui/app_container_view.h"
#import "ios/chrome/browser/scene/ui/scene_view.h"
#import "ios/chrome/browser/scene/ui/scene_view_controller_delegate.h"
#import "ios/chrome/browser/scene/ui/scene_view_delegate.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
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
  _appContentView = [[AppContainerView alloc] init];
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
    [_appContentView.leadingAnchor
        constraintEqualToAnchor:view.leadingAnchor
                       constant:kAppBarHeight - kAppBarCornerRadius],
    [_appContentView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],
    [_appContentView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
  ];
  _landscapeRightConstraints = @[
    [_appContentView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_appContentView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [_appContentView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor
                       constant:-(kAppBarHeight - kAppBarCornerRadius)],
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
  AppBarPosition position = AppBarPositionForView(self.view);
  if (position != AppBarPosition::kBottom) {
    return;
  }
  _fullscreenProgress = progress;
  [self updateLayoutForAppBar];
}

#pragma mark - UIViewController

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)())completion {
  __weak SceneViewController* weakSelf = self;
  [super dismissViewControllerAnimated:flag
                            completion:^() {
                              if (completion) {
                                completion();
                              }
                              [weakSelf showGeminiFloatyIfInvoked];
                            }];
}

#pragma mark - Private


// Updates the layout to adapt to screen changes.
- (void)updateLayoutForAppBar {
  if (!_appBar) {
    return;
  }

  AppBarPosition position = AppBarPositionForView(self.view);
  if (position == AppBarPosition::kNone) {
    return;
  }

  if (!IsFullscreenRefactoringEnabled()) {
    CGRect frame = self.view.bounds;
    UIEdgeInsets insets = UIEdgeInsetsZero;
    switch (position) {
      case AppBarPosition::kLeft:
        insets = UIEdgeInsetsMake(0, kAppBarHeight, 0, 0);
        break;

      case AppBarPosition::kRight:
        insets = UIEdgeInsetsMake(0, 0, 0, kAppBarHeight);
        break;

      case AppBarPosition::kBottom: {
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

  switch (position) {
    case AppBarPosition::kLeft:
      [NSLayoutConstraint activateConstraints:_landscapeLeftConstraints];
      break;

    case AppBarPosition::kRight:
      [NSLayoutConstraint activateConstraints:_landscapeRightConstraints];
      break;

    case AppBarPosition::kBottom:
      [NSLayoutConstraint activateConstraints:_portraitConstraints];
      break;

    default:
      break;
  }

  [self.view layoutIfNeeded];
}

// Helper method for dismissal block when attempting to show the Gemini floaty
// if invoked.
- (void)showGeminiFloatyIfInvoked {
  // Sheet swipe gesture triggers [dismissViewControllerAnimated:completion:].
  // Check if the presented view was truly dismissed which can be implied by
  // `presentedViewController` == nil or the scene is no longer active.
  if (self.presentedViewController ||
      self.view.window.windowScene.activationState !=
          UISceneActivationStateForegroundActive) {
    return;
  }
  [self.delegate sceneViewControllerShowGeminiFloatyIfInvoked:self];
}

@end
