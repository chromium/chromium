// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/coordinator/assistant_container_coordinator.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_animator.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_presenter.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface AssistantContainerCoordinator () <FullscreenUIElement,
                                             FullscreenBrowserAgentObserving>
@end

@implementation AssistantContainerCoordinator {
  // The view controller for the assistant container.
  AssistantContainerViewController* _containerViewController;
  // Observer for the fullscreen controller.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
  // Bridge to observe the FullscreenBrowserAgent.
  std::unique_ptr<FullscreenBrowserAgentObserverBridge>
      _fullscreenBrowserAgentObserverBridge;
  // The content view controller to be displayed inside the container.
  UIViewController* _contentViewController;
  AssistantContainerAnimator* _animator;
  __weak id<AssistantContainerDelegate> _delegate;
  // Whether a dismissal is currently in progress.
  BOOL _dismissalInProgress;
  // Completion block to be executed after dismissal.
  ProceduralBlock _dismissalCompletion;
  // The available detents for the container.
  std::vector<AssistantContainerDetent> _detents;
  // The height for the minimized detent.
  NSInteger _minimizedDetentHeight;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _minimizedDetentHeight = kAssistantContainerMinimizedDetentHeight;
  }
  return self;
}

- (void)start {
  CHECK(self.sceneState.layoutState);
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(AssistantContainerCommands)];
}

- (void)stop {
  NOTREACHED() << "Use stopAnimated:completion: instead.";
}

- (void)stopAnimated:(BOOL)animated completion:(ProceduralBlock)completion {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [self dismissAssistantContainerAnimated:animated completion:completion];
}

#pragma mark - AssistantContainerCommands

- (void)showAssistantContainerWithContent:(UIViewController*)viewController
                                 delegate:
                                     (id<AssistantContainerDelegate>)delegate {
  if (_containerViewController) {
    // Already presented.
    return;
  }

  _contentViewController = viewController;
  _delegate = delegate;
  _animator = [[AssistantContainerAnimator alloc]
      initWithLayoutState:self.sceneState.layoutState];

  _containerViewController = [[AssistantContainerViewController alloc]
      initWithViewController:_contentViewController];
  _containerViewController.delegate = _delegate;
  _containerViewController.minimizedDetentHeight = _minimizedDetentHeight;
  if (!_detents.empty()) {
    _containerViewController.detents = _detents;
  }

  _containerViewController.layoutState = self.sceneState.layoutState;

  // Resolve layout guide.
  GuideName* guideName = kSecondaryToolbarGuide;
  LayoutGuideCenter* center = LayoutGuideCenterForBrowser(self.browser);
  _containerViewController.anchorView = [center referencedViewUnderName:guideName];

  if ([_delegate respondsToSelector:@selector(assistantContainer:
                                              willAppearAnimated:)]) {
    [_delegate assistantContainer:_containerViewController
               willAppearAnimated:YES];
  }

  // Set up fullscreen observation.
  if (IsFullscreenRefactoringEnabled()) {
    FullscreenBrowserAgent* agent =
        FullscreenBrowserAgent::FromBrowser(self.browser);
    _fullscreenBrowserAgentObserverBridge =
        std::make_unique<FullscreenBrowserAgentObserverBridge>(self, agent);
  } else {
    FullscreenController* fullscreenController =
        FullscreenController::FromBrowser(self.browser);
    _fullscreenUIUpdater =
        std::make_unique<FullscreenUIUpdater>(fullscreenController, self);
  }

  __weak __typeof(self) weakSelf = self;
  if (IsUseSceneViewControllerEnabled()) {
    [self.presenter
        addAssistantContainerViewController:_containerViewController];

    if (self.sceneState.layoutState.containedLayoutSupported) {
      [_animator
          animateSidePanelPresentation:_containerViewController
                    baseViewController:self.presenter
                              animated:YES
                            completion:^{
                              [weakSelf didCompletePresentationAnimation];
                            }];
      return;
    }

    [self.baseViewController.view layoutIfNeeded];
    [_animator animatePresentation:_containerViewController
                          animated:YES
                        completion:^{
                          [weakSelf didCompletePresentationAnimation];
                        }];
    return;
  }

  // Add the view controller as a child view controller.
  [self.baseViewController addChildViewController:_containerViewController];

  // Add the view to the hierarchy and apply parent bounds manually.
  UIView* containerView = _containerViewController.view;
  UIView* baseView = self.baseViewController.view;

  [baseView addSubview:containerView];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;

  NSLayoutConstraint* bottomConstraint = [containerView.bottomAnchor
      constraintEqualToAnchor:baseView.bottomAnchor];
  // Lowering priority allows `AssistantContainerViewController` to override
  // the bottom constraint.
  bottomConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [containerView.topAnchor constraintEqualToAnchor:baseView.topAnchor],
    [containerView.leadingAnchor
        constraintEqualToAnchor:baseView.leadingAnchor],
    [containerView.trailingAnchor
        constraintEqualToAnchor:baseView.trailingAnchor],
    bottomConstraint,
  ]];

  [_containerViewController
      didMoveToParentViewController:self.baseViewController];

  // Force layout to determine optimal size before animating.
  [self.baseViewController.view layoutIfNeeded];

  [_animator animatePresentation:_containerViewController
                        animated:YES
                      completion:^{
                        [weakSelf didCompletePresentationAnimation];
                      }];
}

- (void)setAssistantContainerDetents:
    (std::vector<AssistantContainerDetent>)detents {
  _detents = detents;
  [_containerViewController setDetents:detents];
}

- (void)animateAssistantContainerToDetent:(AssistantContainerDetent)detent
                                 duration:(NSTimeInterval)duration
                                    curve:(UIViewAnimationCurve)curve {
  [_containerViewController animateToDetent:detent
                                   duration:duration
                                      curve:curve];
}

- (void)setAssistantContainerMinimizedDetentHeight:(NSInteger)height {
  _minimizedDetentHeight = height;
  if (_containerViewController) {
    _containerViewController.minimizedDetentHeight = height;
  }
}

- (void)dismissAssistantContainerAnimated:(BOOL)animated
                               completion:(ProceduralBlock)completion {
  if (!_containerViewController) {
    if (completion) {
      completion();
    }
    return;
  }

  // If a dismissal is already in progress, update the completion block.
  // If the new request is non-animated, force immediate dismissal.
  if (_dismissalInProgress) {
    if (completion) {
      _dismissalCompletion = completion;
    }
    if (!animated) {
      [_containerViewController.view.layer removeAllAnimations];
      [_containerViewController.view removeFromSuperview];
      [self didCompleteDismissalAnimationAnimated:NO];
    }
    return;
  }

  _dismissalInProgress = YES;
  if (completion) {
    _dismissalCompletion = completion;
  }

  if ([_delegate respondsToSelector:@selector(assistantContainer:
                                           willDisappearAnimated:)]) {
    [_delegate assistantContainer:_containerViewController
            willDisappearAnimated:animated];
  }

  __weak __typeof(self) weakSelf = self;

  if (self.sceneState.layoutState.containedLayoutSupported) {
    [_animator
        animateSidePanelDismissal:_containerViewController
               baseViewController:self.presenter
                         animated:animated
                       completion:^{
                         [weakSelf
                             didCompleteDismissalAnimationAnimated:animated];
                       }];
    return;
  }

  [_animator animateDismissal:_containerViewController
                     animated:animated
                   completion:^{
                     [weakSelf didCompleteDismissalAnimationAnimated:animated];
                   }];
}

#pragma mark - Private

// Called when the presentation animation completes.
- (void)didCompletePresentationAnimation {
  if ([_delegate respondsToSelector:@selector(assistantContainer:
                                               didAppearAnimated:)]) {
    [_delegate assistantContainer:_containerViewController didAppearAnimated:YES];
  }
}

// Called when the dismissal animation completes.
- (void)didCompleteDismissalAnimationAnimated:(BOOL)animated {
  // If the dismissal is not in progress, it means it has already been completed
  // (e.g. by a subsequent non-animated dismissal).
  if (!_dismissalInProgress) {
    return;
  }

  if ([_delegate respondsToSelector:@selector(assistantContainer:
                                            didDisappearAnimated:)]) {
    [_delegate assistantContainer:_containerViewController
             didDisappearAnimated:animated];
  }

  _dismissalInProgress = NO;

  // Cleanup view controller and state.
  _fullscreenUIUpdater = nullptr;
  _fullscreenBrowserAgentObserverBridge = nullptr;

  if (IsUseSceneViewControllerEnabled()) {
    [self.presenter removeAssistantContainerViewController];
  } else {
    [_containerViewController willMoveToParentViewController:nil];
    [_containerViewController.view removeFromSuperview];
    [_containerViewController removeFromParentViewController];
    [_containerViewController didMoveToParentViewController:nil];
  }

  _containerViewController = nil;

  _animator = nil;
  _contentViewController = nil;
  _delegate = nil;
  _detents.clear();

  if (_dismissalCompletion) {
    ProceduralBlock completion = _dismissalCompletion;
    _dismissalCompletion = nil;
    completion();
  }
}

#pragma mark - Accessors

// Returns the presenter by casting the base view controller.
// When the Assistant Side Panel is disabled, the baseVC might not conform to
// this protocol.
- (UIViewController<AssistantContainerPresenter>*)presenter {
  if (IsUseSceneViewControllerEnabled()) {
    CHECK([self.baseViewController
              conformsToProtocol:@protocol(AssistantContainerPresenter)],
          base::NotFatalUntil::M152);
  }
  if ([self.baseViewController
          conformsToProtocol:@protocol(AssistantContainerPresenter)]) {
    return (UIViewController<AssistantContainerPresenter>*)
        self.baseViewController;
  }
  return nil;
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  [_animator animateFullscreenWithProgress:progress
                                animatable:_containerViewController];
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  CGFloat finalProgress = animator.finalProgress;
  __weak __typeof__(self) weakSelf = self;
  [animator addAnimations:^{
    [weakSelf updateForFullscreenProgress:finalProgress];
  }];
}

#pragma mark - FullscreenBrowserAgentObserving

- (void)fullscreenWillUpdateState:(FullscreenBrowserAgent*)agent {
  [self updateForFullscreenProgress:agent->bottom_progress()];
}

@end
