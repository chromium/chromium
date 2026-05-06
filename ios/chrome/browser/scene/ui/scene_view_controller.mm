// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/ui/scene_view_controller.h"

#import <QuartzCore/QuartzCore.h>

#import "base/check.h"
#import "base/trace_event/trace_event.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_presentation_context.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/scene/ui/app_container_view.h"
#import "ios/chrome/browser/scene/ui/scene_mutator.h"
#import "ios/chrome/browser/scene/ui/scene_ui_constants.h"
#import "ios/chrome/browser/scene/ui/scene_view.h"
#import "ios/chrome/browser/scene/ui/scene_view_controller_delegate.h"
#import "ios/chrome/browser/scene/ui/scene_view_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/public/commands/app_bar_commands.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface SceneViewController () <LayoutStateObserver, SceneViewDelegate>
@end

@implementation SceneViewController {
  // The app bar.
  UIViewController* _appBar;
  // The assistant container view controller.
  AssistantContainerViewController* _assistantContainerViewController;

  // Constraints making app content fill the screen for Chrome Next IA.
  NSArray<NSLayoutConstraint*>* _chromeNextIaFillConstraints;

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
  NSLayoutConstraint* _assistantTopConstraint;
  NSLayoutConstraint* _sideAppContentTopConstraint;
  NSLayoutConstraint* _sideAppContentTrailingConstraint;
  NSLayoutConstraint* _sideAppContentBottomConstraint;

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
  _appContentView.accessibilityIdentifier = kAppContentAccessibilityIdentifier;

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
    if (IsChromeNextIaEnabled()) {
      _chromeNextIaFillConstraints = @[
        [_appContentContainerView.leadingAnchor
            constraintEqualToAnchor:view.leadingAnchor],
        [_appContentContainerView.trailingAnchor
            constraintEqualToAnchor:view.trailingAnchor],
        [_appContentContainerView.topAnchor
            constraintEqualToAnchor:view.topAnchor],
        [_appContentContainerView.bottomAnchor
            constraintEqualToAnchor:view.bottomAnchor],
      ];
      [NSLayoutConstraint activateConstraints:_chromeNextIaFillConstraints];
    }
  }

  [self.layoutGuideCenter referenceView:_appContentView
                              underName:kAppContentGuide];

  if (!IsChromeNextIaEnabled() && !IsAssistantSidePanelEnabled()) {
    AddSameConstraints(_appContentContainerView, view);
  }

  view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  [self
      registerForTraitChanges:
          @[ UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class ]
                   withAction:@selector(onSystemTraitChange)];
  [self onSystemTraitChange];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        if (IsChromeNextIaEnabled()) {
          [weakSelf.layoutState updateAppBarPositionWithView:weakSelf.view
                                                 coordinator:coordinator];
        }
      }
                      completion:nil];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.layoutState.windowedMode = IsWindowedMode(self.view.window);
  [self updateAssistantTopConstraints:self.layoutState.containedLayoutActive];
  if (!IsFullscreenRefactoringEnabled()) {
    [self applyFrameForLayout];
  }
}

- (void)viewSafeAreaInsetsDidChange {
  TRACE_EVENT("ui", "-[SceneViewController viewSafeAreaInsetsDidChange]");
  [super viewSafeAreaInsetsDidChange];
  [self updateAssistantTopConstraints:self.layoutState.containedLayoutActive];
  [self.view layoutIfNeeded];
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
  [self updateAssistantVisualStyling:self.layoutState.containedLayoutSupported];
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

#pragma mark - Accessors

- (void)setLayoutState:(LayoutState*)layoutState {
  if (_layoutState == layoutState) {
    return;
  }
  [_layoutState removeObserver:self];
  _layoutState = layoutState;
  [_layoutState addObserver:self];
}

#pragma mark - LayoutStateObserver

- (void)layoutState:(LayoutState*)layoutState
    willChangeContainedLayout:(BOOL)containedLayoutActive
    withTransitionCoordinator:(id<LayoutTransitionCoordinating>)coordinator {
  __weak __typeof(self) weakSelf = self;
  void (^animationBlock)(void) = ^{
    [weakSelf setAssistantContainerVisible:containedLayoutActive];
    [weakSelf setAssistantPanelActive:containedLayoutActive];
  };

  if (coordinator) {
    [coordinator animateAlongsideTransition:animationBlock completion:nil];
  } else {
    animationBlock();
  }
}

- (void)layoutState:(LayoutState*)layoutState
    didChangeContainedLayoutSupported:(BOOL)supported {
  if (supported && _assistantContainerViewController) {
    layoutState.containedLayoutActive = YES;
  } else if (!supported) {
    layoutState.containedLayoutActive = NO;
  }
  [self updateAssistantLayout];
}

- (void)layoutState:(LayoutState*)layoutState
    didChangeWindowedMode:(BOOL)windowedMode {
  [self updateAssistantTopConstraints:self.layoutState.containedLayoutActive];
  [self.view layoutIfNeeded];
}

- (void)layoutState:(LayoutState*)layoutState
    didChangeAppBarPosition:(AppBarPosition)appBarPosition {
  [self updateLayoutForViews];
}

#pragma mark - Private

// This method updates the top constraints for the assistant and app content.
- (void)updateAssistantTopConstraints:(BOOL)active {
  CGFloat constant = 0.0;

  if (active) {
    // The constant equals the safe area inset if anchored to the status bar.
    if (self.view.safeAreaInsets.top > 0) {
      constant = self.view.safeAreaInsets.top;
    } else {
      // Otherwise, it uses the standard container margin.
      constant = kAssistantContainerMargin;
    }
  }

  _assistantTopConstraint.constant = constant;
  _sideAppContentTopConstraint.constant = constant;
}

// Updates the layout state when system traits change.
- (void)onSystemTraitChange {
  self.layoutState.containedLayoutSupported =
      IsSidePanelLayout(self.traitCollection);
  self.layoutState.windowedMode = IsWindowedMode(self.view.window);
}

// Helper to update app content constraints for panel layout.
- (void)updateAppContentConstraintsForPanel:(BOOL)active {
  // We animate constants instead of switching constraint arrays to avoid layout
  // jumps caused by anchor switches (e.g. switching from view.top to
  // safeAreaLayoutGuide.top).
  if (active) {
    CHECK(self.layoutState.appBarPosition == AppBarPosition::kNone);
    [self updateAssistantTopConstraints:active];
    _sideAppContentTrailingConstraint.constant = -kAssistantContainerMargin;
    _sideAppContentBottomConstraint.constant = -kAssistantContainerMargin;
  } else {
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

// Updates both constraints and visual styling for the Assistant container.
- (void)updateAssistantLayout {
  [self updateAssistantLayoutConstraints];
  [self updateAssistantVisualStyling:self.layoutState.containedLayoutSupported];
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
    if (IsChromeNextIaEnabled()) {
      [NSLayoutConstraint activateConstraints:_chromeNextIaFillConstraints];
    }
    return;
  }

  UIView* containerView = _assistantShadowView;
  containerView.translatesAutoresizingMaskIntoConstraints = NO;

  [self setupAssistantPanelConstraints:containerView];
  [self setupAssistantSheetConstraints:containerView];

  if (self.layoutState.containedLayoutSupported) {
    _assistantContainerViewController.presentationContext =
        AssistantPresentationContext::kPanel;
    _activeAssistantConstraints = _assistantPanelConstraints;
    [self updateAssistantTopConstraints:self.layoutState.containedLayoutActive];
    if (IsChromeNextIaEnabled()) {
      [NSLayoutConstraint deactivateConstraints:_chromeNextIaFillConstraints];
    }
  } else {
    _assistantContainerViewController.presentationContext =
        AssistantPresentationContext::kSheet;
    _activeAssistantConstraints = _assistantSheetConstraints;
    if (IsChromeNextIaEnabled()) {
      [NSLayoutConstraint activateConstraints:_chromeNextIaFillConstraints];
    }
  }

  [NSLayoutConstraint activateConstraints:_activeAssistantConstraints];
}

// Applies visual aesthetics depending on whether the side panel layout is
// active.
- (void)updateAssistantVisualStyling:(BOOL)active {
  if (!IsAssistantSidePanelEnabled() || !self.view.window) {
    return;
  }

  if (_assistantContainerViewController) {
    UIView* assistantView = _assistantContainerViewController.view;
    ApplyAssistantSidePanelAesthetics(assistantView, _assistantShadowView,
                                      active);
  }

  ApplyAssistantSidePanelAesthetics(_appContentView, _appContentContainerView,
                                    active);
}

// Updates the layout of the scene views depending on the active layout strategy
// (Constraints vs. Frames).
- (void)updateLayoutForViews {
  AppBarPosition position = self.layoutState.appBarPosition;
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

  // Ensure default constraints are active to avoid leaving the view
  // unconstrained if `_appBar` is hidden or missing.
  if (position == AppBarPosition::kNone || !_appBar) {
    if (!_assistantContainerViewController) {
      [self setupDefaultConstraints];
      [NSLayoutConstraint activateConstraints:_baseAssistantConstraints];
    }
    return;
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

  if (self.layoutState.containedLayoutSupported &&
      _assistantContainerViewController && panelWidth > 0) {
    CGFloat safeAreaTop = self.view.safeAreaInsets.top;
    CGFloat margin = _assistantVisible ? kAssistantContainerMargin : 0.0;
    // The base top inset uses the safe area if anchored to the status bar,
    // otherwise it uses the container margin.
    CGFloat baseTop =
        (safeAreaTop > 0) ? safeAreaTop : kAssistantContainerMargin;

    insets.right += margin;
    insets.top += _assistantVisible ? baseTop : 0.0;
    insets.bottom += margin;
  }

  CGRect contentFrame = UIEdgeInsetsInsetRect(frame, insets);
  _appContentContainerView.frame = contentFrame;
  if (self.layoutState.containedLayoutActive && !IsChromeNextIaEnabled()) {
    // When the Assistant side panel is active, use bounds to avoid a double
    // shift of the origin.
    // However, when IsChromeNextIaEnabled() is true, the view hierarchy is
    // inverted (when fullscreen refactor is disabled). In that mode, we must
    // use frame to avoid misplacing the view at the left edge.
    _appContentView.frame = _appContentContainerView.bounds;
    return;
  }
  _appContentView.frame = _appContentContainerView.frame;
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
  AppBarPosition position = self.layoutState.appBarPosition;
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
  if (!self.layoutState.containedLayoutSupported ||
      !_assistantContainerViewController) {
    return 0;
  }

  CGFloat panelWidth = [self assistantSidePanelWidth];
  if (panelWidth <= 0) {
    return 0;
  }

  CGFloat width = panelWidth + _assistantLeadingConstraint.constant;
  return MAX(0, width + kAssistantContainerMargin);
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
                     constant:-kAssistantSidePanelMaxWidth];

  _assistantTopConstraint =
      [assistantView.topAnchor constraintEqualToAnchor:view.topAnchor
                                              constant:0];

  NSLayoutConstraint* proportionalWidth = [assistantView.widthAnchor
      constraintEqualToAnchor:view.widthAnchor
                   multiplier:kAssistantSidePanelWidthMultiplier];
  proportionalWidth.priority = UILayoutPriorityRequired - 1;

  _assistantPanelConstraints = @[
    _assistantLeadingConstraint,
    _assistantTopConstraint,
    [assistantView.bottomAnchor
        constraintEqualToAnchor:view.bottomAnchor
                       constant:-kAssistantContainerMargin],
    proportionalWidth,
    [assistantView.widthAnchor
        constraintLessThanOrEqualToConstant:kAssistantSidePanelMaxWidth],
  ];

  if (IsFullscreenRefactoringEnabled()) {
    [self setupAppContentConstraintsForPanel:assistantView];
    [self updateAssistantTopConstraints:self.layoutState.containedLayoutActive];
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

  NSArray* appContentConstraints = @[
    [_appContentContainerView.leadingAnchor
        constraintEqualToAnchor:assistantView.trailingAnchor
                       constant:kAssistantContainerMargin],
    _sideAppContentTrailingConstraint,
    _sideAppContentBottomConstraint,
    _sideAppContentTopConstraint,
  ];

  _assistantPanelConstraints = [_assistantPanelConstraints
      arrayByAddingObjectsFromArray:appContentConstraints];
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
  if (IsFullscreenRefactoringEnabled() && !IsChromeNextIaEnabled()) {
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

#pragma mark - SceneConsumer

- (void)showNewIAPromo {
  [self.appBarHandler showIPHBackground];
  BubbleArrowDirection arrowDirection = BubbleArrowDirectionDown;
  AppBarPosition position = self.layoutState.appBarPosition;
  if (position == AppBarPosition::kLeft) {
    arrowDirection = BubbleArrowDirectionLeading;
  } else if (position == AppBarPosition::kRight) {
    arrowDirection = BubbleArrowDirectionTrailing;
  }

  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType callback =
      ^(IPHDismissalReasonType reason) {
        [weakSelf.appBarHandler hideIPHBackground];
        [weakSelf.mutator newIAPromoIPHDismissed];
      };

  NSString* title = l10n_util::GetNSString(IDS_IOS_NEW_IA_PROMO_IPH_TITLE);
  NSString* subtitle = l10n_util::GetNSString(IDS_IOS_NEW_IA_PROMO_IPH_TEXT);

  BubbleViewControllerPresenter* presenter =
      [[BubbleViewControllerPresenter alloc]
               initWithText:subtitle
                      title:title
             arrowDirection:arrowDirection
                  alignment:BubbleAlignmentCenter
                 bubbleType:BubbleViewTypeRich
            pageControlPage:BubblePageControlPageNone
          dismissalCallback:callback];

  UIView* anchorView =
      [self.layoutGuideCenter referencedViewUnderName:kAppBarGuide];
  if (!anchorView) {
    anchorView = self.view;
  }

  // `convertPoint:toView:` is taking into account the transform. In all cases,
  // the closest side to the content is the top side.
  CGPoint anchorPoint = CGPointMake(anchorView.bounds.size.width / 2.0, 0);
  CGPoint windowAnchorPoint = [anchorView convertPoint:anchorPoint toView:nil];

  [presenter presentInViewController:self anchorPoint:windowAnchorPoint];
}

@end
