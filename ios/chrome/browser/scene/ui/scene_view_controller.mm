// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/ui/scene_view_controller.h"

#import <QuartzCore/QuartzCore.h>

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
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface SceneViewController () <SceneViewDelegate>
@end

@implementation SceneViewController {
  // The app bar.
  UIViewController* _appBar;
  // The assistant container view controller.
  AssistantContainerViewController* _assistantContainerViewController;

  // Container for App Content view to handle shadow for Side Panel floating
  // card effect.
  UIView* _appContentContainerView;

  // The view containing the app content (the part outside the app bar).
  UIView* _appContentView;

  // Container for Assistant view to handle shadow for Side Panel floating card
  // effect.
  UIView* _assistantShadowView;

  // The Assistant constraints.
  NSArray<NSLayoutConstraint*>* _baseAssistantConstraints;
  NSArray<NSLayoutConstraint*>* _activeAssistantConstraints;
  NSArray<NSLayoutConstraint*>* _assistantSheetConstraints;
  NSArray<NSLayoutConstraint*>* _assistantPanelConstraints;
  NSLayoutConstraint* _assistantLeadingConstraint;
  NSLayoutConstraint* _sideAppContentTopConstraint;
  NSLayoutConstraint* _sideAppContentTrailingConstraint;
  NSLayoutConstraint* _sideAppContentBottomConstraint;

  // App bar constraints.
  NSArray<NSLayoutConstraint*>* _portraitConstraints;
  NSArray<NSLayoutConstraint*>* _landscapeLeftConstraints;
  NSArray<NSLayoutConstraint*>* _landscapeRightConstraints;

  // The last fullscreen progress value received.
  CGFloat _fullscreenProgress;
  // Whether the assistant container is visible.
  BOOL _assistantVisible;
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
  _appContentContainerView = [[UIView alloc] init];
  _appContentView = [[AppContainerView alloc] init];
  _appContentView.clipsToBounds = YES;

  if (IsFullscreenRefactoringEnabled()) {
    _appContentContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _appContentView.translatesAutoresizingMaskIntoConstraints = NO;
  } else {
    _appContentContainerView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _appContentView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }

  [view addSubview:_appContentContainerView];
  _appContentContainerView.frame = view.bounds;

  [_appContentContainerView addSubview:_appContentView];
  _appContentView.frame = _appContentContainerView.bounds;

  if (IsFullscreenRefactoringEnabled()) {
    AddSameConstraints(_appContentView, _appContentContainerView);
  }

  [self.layoutGuideCenter referenceView:_appContentView
                              underName:kAppContentGuide];

  if (!IsChromeNextIaEnabled() && !IsAssistantSidePanelEnabled()) {
    AddSameConstraints(_appContentContainerView, view);
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

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (!IsFullscreenRefactoringEnabled()) {
    [self applyFrameForLayout];
  }
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

  [self setupAppBarView:appBar];

  if (!IsFullscreenRefactoringEnabled()) {
    [self updateLayoutForViews];
    return;
  }

  [self setupAppBarConstraints];
  [self updateLayoutForViews];
}

#pragma mark - SceneViewDelegate

- (void)sceneViewDidMoveToWindow:(SceneView*)sceneView {
  [self updateLayoutForViews];
  [self updateAssistantLayout];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  if (progress == _fullscreenProgress) {
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

#pragma mark - AssistantContainerPresenter

- (void)addAssistantContainerViewController:
    (AssistantContainerViewController*)assistantContainerViewController {
  CHECK(assistantContainerViewController);
  CHECK(!_assistantContainerViewController);

  _assistantContainerViewController = assistantContainerViewController;

  _assistantShadowView = [[UIView alloc] init];
  _assistantShadowView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_assistantShadowView];

  [self addChildViewController:_assistantContainerViewController];
  [_assistantShadowView addSubview:_assistantContainerViewController.view];
  _assistantContainerViewController.view.clipsToBounds = YES;
  [_assistantContainerViewController didMoveToParentViewController:self];

  _assistantContainerViewController.view
      .translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(_assistantContainerViewController.view,
                     _assistantShadowView);

  [self updateAssistantLayoutConstraints];
  [self updateAssistantVisualStyling:IsSidePanelLayout(self.traitCollection)];
}

- (void)removeAssistantContainerViewController {
  if (!_assistantContainerViewController) {
    return;
  }

  [_assistantShadowView removeFromSuperview];
  _assistantShadowView = nil;

  if (_activeAssistantConstraints) {
    [NSLayoutConstraint deactivateConstraints:_activeAssistantConstraints];
    _activeAssistantConstraints = nil;
  }
  _assistantSheetConstraints = nil;
  _assistantPanelConstraints = nil;

  [_assistantContainerViewController willMoveToParentViewController:nil];
  [_assistantContainerViewController.view removeFromSuperview];
  [_assistantContainerViewController removeFromParentViewController];
  _assistantContainerViewController = nil;

  // Resets constraints to default.
  [self updateAssistantLayoutConstraints];

  // Resets aesthetics.
  [self updateAssistantVisualStyling:NO];
}

- (void)setAssistantContainerVisible:(BOOL)visible {
  _assistantVisible = visible;
  UIView* assistantView = _assistantShadowView;
  if (!assistantView) {
    return;
  }
  CGFloat width = assistantView.frame.size.width;

  // Add margin to ensure the shadow doesn't bleed onto the screen.
  CGFloat startOffset = -(width + kAssistantContainerMargin);
  CGFloat endOffset = kAssistantContainerMargin;

  CGFloat currentOffset = visible ? endOffset : startOffset;
  _assistantLeadingConstraint.constant = currentOffset;

  [self.view layoutIfNeeded];

  if (IsFullscreenRefactoringEnabled()) {
    [self updateLayoutForViews];
  } else {
    [self applyFrameForAppContentLayout];
  }
}

- (void)setAssistantPanelActive:(BOOL)active {
  [self updateAssistantVisualStyling:active];
  if (IsFullscreenRefactoringEnabled()) {
    [self updateAppContentConstraintsForPanel:active];
  }
}

#pragma mark - Private

// Helper to update app content constraints for panel layout.
- (void)updateAppContentConstraintsForPanel:(BOOL)active {
  // We animate constants instead of switching constraint arrays to avoid layout
  // jumps caused by anchor switches (e.g. switching from view.top to
  // safeAreaLayoutGuide.top).
  if (active) {
    CHECK(AppBarPositionForView(self.view) == AppBarPosition::kNone);
    CGFloat safeAreaTop = self.view.safeAreaInsets.top;
    _sideAppContentTopConstraint.constant =
        safeAreaTop + kAssistantContainerMargin;
    _sideAppContentTrailingConstraint.constant = -kAssistantContainerMargin;
    _sideAppContentBottomConstraint.constant = -kAssistantContainerMargin;
  } else {
    _sideAppContentTopConstraint.constant = 0;
    _sideAppContentTrailingConstraint.constant = 0;
    _sideAppContentBottomConstraint.constant = 0;
  }
  [self.view layoutIfNeeded];
}

// Sets up and positions the App Bar view.
- (void)setupAppBarView:(UIViewController*)appBar {
  UIView* appBarView = appBar.view;
  appBarView.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* view = self.view;

  [self addChildViewController:appBar];
  [view addSubview:appBarView];

  AddSameCenterConstraints(view, appBarView);

  [appBar didMoveToParentViewController:self];
}

// Sets up the Auto Layout constraints for the App Bar.
- (void)setupAppBarConstraints {
  UIView* view = self.view;

  _portraitConstraints = @[
    [_appContentContainerView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_appContentContainerView.leadingAnchor
        constraintEqualToAnchor:view.leadingAnchor],
    [_appContentContainerView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],
    [_appContentContainerView.bottomAnchor
        constraintEqualToAnchor:view.bottomAnchor],
  ];
  _landscapeLeftConstraints = @[
    [_appContentContainerView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_appContentContainerView.leadingAnchor
        constraintEqualToAnchor:view.leadingAnchor
                       constant:kAppBarHeight - kAppBarCornerRadius],
    [_appContentContainerView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],
    [_appContentContainerView.bottomAnchor
        constraintEqualToAnchor:view.bottomAnchor],
  ];
  _landscapeRightConstraints = @[
    [_appContentContainerView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_appContentContainerView.leadingAnchor
        constraintEqualToAnchor:view.leadingAnchor],
    [_appContentContainerView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor
                       constant:-(kAppBarHeight - kAppBarCornerRadius)],
    [_appContentContainerView.bottomAnchor
        constraintEqualToAnchor:view.bottomAnchor],
  ];
}

// Updates both constraints and visual styling for the Assistant container.
- (void)updateAssistantLayout {
  [self updateAssistantLayoutConstraints];
  [self updateAssistantVisualStyling:IsSidePanelLayout(self.traitCollection)];
}

// Updates the active assistant constraints for the current active layout.
- (void)updateAssistantLayoutConstraints {
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

  UIView* containerView = _assistantShadowView;
  containerView.translatesAutoresizingMaskIntoConstraints = NO;

  [self setupAssistantPanelConstraints:containerView];
  [self setupAssistantSheetConstraints:containerView];

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

// Applies visual aesthetics depending on whether the side panel layout is
// active.
- (void)updateAssistantVisualStyling:(BOOL)active {
  if (!IsAssistantSidePanelEnabled() || !self.view.window) {
    return;
  }

  UIView* view = self.view;

  if (_assistantContainerViewController) {
    UIView* assistantView = _assistantContainerViewController.view;
    ApplyAssistantSidePanelAesthetics(assistantView, _assistantShadowView,
                                      active);
  }

  ApplyAssistantSidePanelAesthetics(_appContentView, _appContentContainerView,
                                    active);
  view.backgroundColor =
      active ? [UIColor colorNamed:kSecondaryBackgroundColor] : nil;
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
- (void)applyFrameForAppContentLayout {
  CGRect frame = self.view.bounds;
  UIEdgeInsets insets = [self appBarInsets];
  insets.left += [self sidePanelLeftInset];

  CGFloat panelWidth = [self assistantSidePanelWidth];

  if (IsSidePanelLayout(self.traitCollection) &&
      _assistantContainerViewController && panelWidth > 0) {
    CGFloat safeAreaTop = self.view.safeAreaInsets.top;
    CGFloat margin = _assistantVisible ? kAssistantContainerMargin : 0.0;

    insets.right += margin;
    insets.top +=
        _assistantVisible ? (safeAreaTop + kAssistantContainerMargin) : 0.0;
    insets.bottom += margin;
  }

  CGRect contentFrame = UIEdgeInsetsInsetRect(frame, insets);
  _appContentView.frame = contentFrame;
  _appContentContainerView.frame = contentFrame;
}

// Applies manual frames to views. This is the fallback layout path when
// fullscreen refactoring is disabled.
- (void)applyFrameForLayout {
  [self applyFrameForAppContentLayout];
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

// Calculates the width of the Assistant Side Panel based on current bounds.
- (CGFloat)assistantSidePanelWidth {
  return MIN(self.view.bounds.size.width * kAssistantSidePanelWidthMultiplier,
             kAssistantSidePanelMaxWidth);
}

// Calculates left inset for the Assistant Side Panel.
- (CGFloat)sidePanelLeftInset {
  if (!IsSidePanelLayout(self.traitCollection) ||
      !_assistantContainerViewController) {
    return 0;
  }

  CGFloat panelWidth = [self assistantSidePanelWidth];
  if (panelWidth <= 0) {
    return 0;
  }

  CGFloat width = panelWidth + _assistantLeadingConstraint.constant;
  return MAX(0, width + (_assistantVisible ? kAssistantContainerMargin : 0.0));
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
      [_appContentContainerView.leadingAnchor
          constraintEqualToAnchor:view.leadingAnchor],
      [_appContentContainerView.trailingAnchor
          constraintEqualToAnchor:view.trailingAnchor],
      [_appContentContainerView.topAnchor
          constraintEqualToAnchor:view.topAnchor],
      [_appContentContainerView.bottomAnchor
          constraintEqualToAnchor:view.bottomAnchor],
    ];
  }
}

// Sets up panel constraints for iPad side panel layout.
- (void)setupAssistantPanelConstraints:(UIView*)assistantView {
  if (_assistantPanelConstraints) {
    return;
  }
  UIView* view = self.view;
  _assistantLeadingConstraint = [assistantView.leadingAnchor
      constraintEqualToAnchor:view.safeAreaLayoutGuide.leadingAnchor
                     constant:kAssistantContainerMargin];

  NSArray* panelConstraints = @[
    _assistantLeadingConstraint,
    [assistantView.topAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.topAnchor
                       constant:kAssistantContainerMargin],
    [assistantView.bottomAnchor
        constraintEqualToAnchor:view.bottomAnchor
                       constant:-kAssistantContainerMargin],
    [assistantView.widthAnchor
        constraintEqualToAnchor:view.widthAnchor
                     multiplier:kAssistantSidePanelWidthMultiplier],
    [assistantView.widthAnchor
        constraintLessThanOrEqualToConstant:kAssistantSidePanelMaxWidth],
  ];

  _assistantPanelConstraints = panelConstraints;
  if (IsFullscreenRefactoringEnabled()) {
    [self setupAppContentConstraintsForPanel:assistantView];
  }
}

// Sets up constraints for the app content view when the assistant side panel is
// active.
- (void)setupAppContentConstraintsForPanel:(UIView*)assistantView {
  UIView* view = self.view;
  _sideAppContentTopConstraint =
      [_appContentContainerView.topAnchor constraintEqualToAnchor:view.topAnchor
                                                         constant:0];
  _sideAppContentTrailingConstraint = [_appContentContainerView.trailingAnchor
      constraintEqualToAnchor:view.trailingAnchor
                     constant:0];
  _sideAppContentBottomConstraint = [_appContentContainerView.bottomAnchor
      constraintEqualToAnchor:view.bottomAnchor
                     constant:0];

  _assistantPanelConstraints =
      [_assistantPanelConstraints arrayByAddingObjectsFromArray:@[
        [_appContentContainerView.leadingAnchor
            constraintEqualToAnchor:assistantView.trailingAnchor
                           constant:kAssistantContainerMargin],
        _sideAppContentTrailingConstraint,
        _sideAppContentTopConstraint,
        _sideAppContentBottomConstraint,
      ]];
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
          [_appContentContainerView.leadingAnchor
              constraintEqualToAnchor:view.leadingAnchor],
          [_appContentContainerView.trailingAnchor
              constraintEqualToAnchor:view.trailingAnchor],
          [_appContentContainerView.topAnchor
              constraintEqualToAnchor:view.topAnchor],
          [_appContentContainerView.bottomAnchor
              constraintEqualToAnchor:view.bottomAnchor],
        ]];
  }
}

@end
