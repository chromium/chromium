// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/ui/scene_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_utils.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_presentation_context.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/scene/ui/app_container_view.h"
#import "ios/chrome/browser/scene/ui/scene_view.h"
#import "ios/chrome/browser/scene/ui/scene_view_controller_delegate.h"
#import "ios/chrome/browser/scene/ui/scene_view_delegate.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface SceneViewController () <SceneViewDelegate>

@end

@implementation SceneViewController {
  // The app bar.
  UIViewController* _appBar;
  // The assistant container view controller.
  AssistantContainerViewController* _assistantContainerViewController;

  // The view containing the app (the part outside the app bar).
  UIView* _appContentView;

  // The Assistant constraints.
  NSArray<NSLayoutConstraint*>* _baseAssistantConstraints;
  NSArray<NSLayoutConstraint*>* _activeAssistantConstraints;
  NSArray<NSLayoutConstraint*>* _assistantSheetConstraints;
  NSArray<NSLayoutConstraint*>* _assistantPanelConstraints;
  NSLayoutConstraint* _assistantLeadingConstraint;

  // App bar constraints.
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

  if (!IsChromeNextIaEnabled() && !IsAssistantSidePanelEnabled()) {
    AddSameConstraints(_appContentView, view);
  }

  [self
      registerForTraitChanges:
          @[ UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class ]
                   withAction:@selector(updateAssistantLayout)];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf updateLayoutForViews];
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

  if (!IsFullscreenRefactoringEnabled()) {
    [self updateLayoutForViews];
    return;
  }

  _portraitConstraints = @[
    [_appContentView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_appContentView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [_appContentView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],
    [_appContentView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
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

  [self updateLayoutForViews];
}

#pragma mark - SceneViewDelegate

- (void)sceneViewDidMoveToWindow:(SceneView*)sceneView {
  [self updateLayoutForViews];
  [self updateAssistantLayout];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  AppBarPosition position = AppBarPositionForView(self.view);
  if (position != AppBarPosition::kBottom) {
    return;
  }
  _fullscreenProgress = progress;
  [self updateLayoutForViews];
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

#pragma mark - AssistantContainerProvider

- (void)addAssistantContainerViewController:
    (AssistantContainerViewController*)assistantContainerViewController {
  CHECK(assistantContainerViewController);
  CHECK(!_assistantContainerViewController);

  _assistantContainerViewController = assistantContainerViewController;

  [self addChildViewController:_assistantContainerViewController];
  [self.view addSubview:_assistantContainerViewController.view];
  [_assistantContainerViewController didMoveToParentViewController:self];

  [self updateAssistantLayout];
}

- (void)removeAssistantContainerViewController {
  if (!_assistantContainerViewController) {
    return;
  }

  _assistantSheetConstraints = nil;
  _assistantPanelConstraints = nil;
  [_assistantContainerViewController willMoveToParentViewController:nil];
  [_assistantContainerViewController.view removeFromSuperview];
  [_assistantContainerViewController removeFromParentViewController];
  _assistantContainerViewController = nil;

  [self updateAssistantLayout];
}

- (void)updateAssistantContainerOffset:(CGFloat)offset {
  _assistantLeadingConstraint.constant = offset;
  [self.view layoutIfNeeded];
  [self updateLayoutForViews];
}

#pragma mark - Private

// Updates the active assistant constraints for the current active layout.
- (void)updateAssistantLayout {
  if (!IsAssistantSidePanelEnabled() || !self.view.window) {
    return;
  }

  [NSLayoutConstraint deactivateConstraints:_activeAssistantConstraints];
  _activeAssistantConstraints = nil;

  if (!_assistantContainerViewController) {
    _assistantLeadingConstraint = nil;
    [self setupDefaultConstraints];
    _activeAssistantConstraints = _baseAssistantConstraints;
    [NSLayoutConstraint activateConstraints:_activeAssistantConstraints];
    return;
  }

  UIView* assistantView = _assistantContainerViewController.view;
  assistantView.translatesAutoresizingMaskIntoConstraints = NO;

  [self setupAssistantPanelConstraints:assistantView];
  [self setupAssistantSheetConstraints:assistantView];

  if (IsSidePanelLayout(self.traitCollection)) {
    _assistantContainerViewController.presentationContext =
        AssistantPresentationContext::kPanel;
    _activeAssistantConstraints = _assistantPanelConstraints;
  } else {
    _assistantContainerViewController.presentationContext =
        AssistantPresentationContext::kSheet;
    _activeAssistantConstraints = _assistantSheetConstraints;
  }

  [NSLayoutConstraint activateConstraints:_activeAssistantConstraints];
}

// Updates the layout of the scene views depending on the active layout strategy
// (Constraints vs. Frames).
- (void)updateLayoutForViews {
  AppBarPosition position = AppBarPositionForView(self.view);
  _appBar.view.hidden = (position == AppBarPosition::kNone);
  if (IsFullscreenRefactoringEnabled()) {
    [self applyConstraintsForLayoutWithPosition:position];
  } else {
    [self applyFrameForLayout];
  }
}

// Applies Auto Layout constraints to views.
- (void)applyConstraintsForLayoutWithPosition:(AppBarPosition)position {
  UIView* view = self.view;

  [NSLayoutConstraint deactivateConstraints:_portraitConstraints];
  [NSLayoutConstraint deactivateConstraints:_landscapeLeftConstraints];
  [NSLayoutConstraint deactivateConstraints:_landscapeRightConstraints];
  [NSLayoutConstraint deactivateConstraints:_baseAssistantConstraints];

  // Ensure default constraints are active to avoid leaving the view
  // unconstrained if `_appBar` is hidden or missing.
  if (position == AppBarPosition::kNone || !_appBar) {
    if (!_assistantContainerViewController) {
      [self setupDefaultConstraints];
      [NSLayoutConstraint activateConstraints:_baseAssistantConstraints];
    }
    return;
  }

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

  [view layoutIfNeeded];
}

// Applies manual frames to views by combining insets from App Bar and Side
// Panel features.
- (void)applyFrameForLayout {
  CGRect frame = self.view.bounds;
  UIEdgeInsets insets = [self appBarInsets];
  insets.left += [self sidePanelLeftInset];

  _appContentView.frame = UIEdgeInsetsInsetRect(frame, insets);
}

// Calculates insets for the App Bar.
- (UIEdgeInsets)appBarInsets {
  if (!_appBar) {
    return UIEdgeInsetsZero;
  }
  AppBarPosition position = AppBarPositionForView(self.view);
  if (position == AppBarPosition::kNone) {
    return UIEdgeInsetsZero;
  }

  UIEdgeInsets insets = UIEdgeInsetsZero;
  switch (position) {
    case AppBarPosition::kLeft:
      insets.left += kAppBarHeight;
      break;

    case AppBarPosition::kRight:
      insets.right += kAppBarHeight;
      break;

    case AppBarPosition::kBottom: {
      CGFloat appBarHeight =
          kAppBarHeightFullscreen -
          _fullscreenProgress * (kAppBarHeightFullscreen - kAppBarHeight);
      insets.bottom += appBarHeight;
      break;
    }

    default:
      break;
  }
  return insets;
}

// Calculates left inset for the Assistant Side Panel.
- (CGFloat)sidePanelLeftInset {
  if (!IsSidePanelLayout(self.traitCollection) ||
      !_assistantContainerViewController) {
    return 0;
  }

  CGFloat width = _assistantContainerViewController.view.bounds.size.width +
                  _assistantLeadingConstraint.constant;
  return MAX(0, width);
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

// Sets up default constraints when no assistant is present.
- (void)setupDefaultConstraints {
  if (_baseAssistantConstraints) {
    return;
  }
  _baseAssistantConstraints = @[];
  if (IsFullscreenRefactoringEnabled()) {
    UIView* view = self.view;
    _baseAssistantConstraints = @[
      [_appContentView.leadingAnchor
          constraintEqualToAnchor:view.leadingAnchor],
      [_appContentView.trailingAnchor
          constraintEqualToAnchor:view.trailingAnchor],
      [_appContentView.topAnchor constraintEqualToAnchor:view.topAnchor],
      [_appContentView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    ];
  }
}

// Sets up panel constraints for iPad side panel layout.
- (void)setupAssistantPanelConstraints:(UIView*)assistantView {
  if (_assistantPanelConstraints) {
    return;
  }
  UIView* view = self.view;
  _assistantLeadingConstraint =
      [assistantView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor];

  NSArray* panelConstraints = @[
    _assistantLeadingConstraint,
    [assistantView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [assistantView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [assistantView.widthAnchor
        constraintEqualToAnchor:view.widthAnchor
                     multiplier:kAssistantSidePanelWidthMultiplier],
    [assistantView.widthAnchor
        constraintLessThanOrEqualToConstant:kAssistantSidePanelMaxWidth],
  ];

  _assistantPanelConstraints = panelConstraints;
  if (IsFullscreenRefactoringEnabled()) {
    _assistantPanelConstraints =
        [panelConstraints arrayByAddingObjectsFromArray:@[
          [_appContentView.leadingAnchor
              constraintEqualToAnchor:assistantView.trailingAnchor],
          [_appContentView.trailingAnchor
              constraintEqualToAnchor:view.trailingAnchor],
          [_appContentView.topAnchor constraintEqualToAnchor:view.topAnchor],
          [_appContentView.bottomAnchor
              constraintEqualToAnchor:view.bottomAnchor],
        ]];
  }
}

// Sets up sheet constraints for bottom sheet layout.
- (void)setupAssistantSheetConstraints:(UIView*)assistantView {
  if (_assistantSheetConstraints) {
    return;
  }
  UIView* view = self.view;
  NSLayoutConstraint* bottomConstraint =
      [assistantView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor];
  // Lowering priority allows `AssistantContainerViewController` to override
  // the bottom constraint.
  bottomConstraint.priority = UILayoutPriorityDefaultHigh;

  NSArray* sheetConstraints = @[
    [assistantView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [assistantView.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
    [assistantView.topAnchor constraintEqualToAnchor:view.topAnchor],
    bottomConstraint,
  ];

  _assistantSheetConstraints = sheetConstraints;
  if (IsFullscreenRefactoringEnabled()) {
    _assistantSheetConstraints =
        [sheetConstraints arrayByAddingObjectsFromArray:@[
          [_appContentView.leadingAnchor
              constraintEqualToAnchor:view.leadingAnchor],
          [_appContentView.trailingAnchor
              constraintEqualToAnchor:view.trailingAnchor],
          [_appContentView.topAnchor constraintEqualToAnchor:view.topAnchor],
          [_appContentView.bottomAnchor
              constraintEqualToAnchor:view.bottomAnchor],
        ]];
  }
}

@end
