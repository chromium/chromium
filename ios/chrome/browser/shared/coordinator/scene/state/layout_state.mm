// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"

#import <cmath>

#import "base/ios/crb_protocol_observers.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// Calculates the target `UIInterfaceOrientation` by applying the rotation angle
// extracted from the transition coordinator's `targetTransform` onto the
// current orientation's layout angle.
//
// During a size transition (such as screen rotation), the window scene's
// `interfaceOrientation` has not yet updated to its final state inside
// `-viewWillTransitionToSize:withTransitionCoordinator:`. To allow observers to
// configure layouts and update constraint constants in sync with the rotation
// animation, the target orientation must be pre-calculated using the transition
// coordinator's target transform. This extra work is required on iOS 18, but
// is not needed on iOS 26 and newer where the scene's `effectiveGeometry` is
// already reliable and updated.
UIInterfaceOrientation GetTargetInterfaceOrientation(
    UIInterfaceOrientation current_orientation,
    CGAffineTransform target_transform) {
  CGFloat current_angle = 0;
  switch (current_orientation) {
    case UIInterfaceOrientationLandscapeLeft:
      current_angle = -M_PI_2;
      break;
    case UIInterfaceOrientationLandscapeRight:
      current_angle = M_PI_2;
      break;
    case UIInterfaceOrientationPortraitUpsideDown:
      current_angle = M_PI;
      break;
    default:
      current_angle = 0;
      break;
  }

  CGFloat delta_angle = atan2(target_transform.b, target_transform.a);
  CGFloat target_angle = current_angle + delta_angle;

  while (target_angle > M_PI) {
    target_angle -= 2 * M_PI;
  }
  while (target_angle <= -M_PI) {
    target_angle += 2 * M_PI;
  }

  const CGFloat epsilon = 0.1;
  if (std::abs(target_angle) < epsilon) {
    return UIInterfaceOrientationPortrait;
  } else if (std::abs(target_angle - M_PI_2) < epsilon) {
    return UIInterfaceOrientationLandscapeRight;
  } else if (std::abs(target_angle + M_PI_2) < epsilon) {
    return UIInterfaceOrientationLandscapeLeft;
  } else if (std::abs(std::abs(target_angle) - M_PI) < epsilon) {
    return UIInterfaceOrientationPortraitUpsideDown;
  }

  return UIInterfaceOrientationUnknown;
}

}  // namespace

@interface LayoutStateObserverList : CRBProtocolObservers <LayoutStateObserver>
@end

@implementation LayoutStateObserverList
@end

@implementation LayoutState {
  LayoutStateObserverList* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [LayoutStateObserverList
        observersWithProtocol:@protocol(LayoutStateObserver)];
  }
  return self;
}

#pragma mark - PassKey Setters

- (void)setContainedLayoutActive:(BOOL)active
                    scenePassKey:(LayoutStateScenePassKey)passKey {
  if (_containedLayoutActive == active) {
    return;
  }
  [_observers layoutState:self
      willChangeContainedLayout:active
      withTransitionCoordinator:nil];
  _containedLayoutActive = active;
}

- (void)setContainedLayoutActive:(BOOL)active
                assistantPassKey:(LayoutStateAssistantPassKey)passKey {
  if (_containedLayoutActive == active) {
    return;
  }
  [_observers layoutState:self
      willChangeContainedLayout:active
      withTransitionCoordinator:nil];
  _containedLayoutActive = active;
}

- (void)setContainedLayoutSupported:(BOOL)supported
                            passKey:(LayoutStateScenePassKey)passKey {
  if (_containedLayoutSupported == supported) {
    return;
  }
  _containedLayoutSupported = supported;
  [_observers layoutState:self didChangeContainedLayoutSupported:supported];
}

- (void)setWindowedMode:(BOOL)windowedMode
                passKey:(LayoutStateScenePassKey)passKey {
  if (_windowedMode == windowedMode) {
    return;
  }
  _windowedMode = windowedMode;
  [_observers layoutState:self didChangeWindowedMode:windowedMode];
}

- (void)setAssistantContainerCutoutRadius:(CGFloat)radius
                                  passKey:(LayoutStateAssistantPassKey)passKey {
  if (_assistantContainerCutoutRadius == radius) {
    return;
  }
  _assistantContainerCutoutRadius = radius;
  [_observers layoutState:self didChangeAssistantContainerCutoutRadius:radius];
}

- (void)setAppBarLockedInFullscreen:(BOOL)locked
                            passKey:(LayoutStateAssistantPassKey)passKey {
  if (_appBarLockedInFullscreen == locked) {
    return;
  }
  _appBarLockedInFullscreen = locked;
  [_observers layoutState:self didChangeAppBarLockedInFullscreen:locked];
}

- (void)setGeminiFloatyInvoked:(BOOL)invoked
                       passKey:(LayoutStateAssistantPassKey)passKey {
  if (_geminiFloatyInvoked == invoked) {
    return;
  }
  _geminiFloatyInvoked = invoked;
  [_observers layoutState:self didChangeGeminiFloatyInvoked:invoked];
}

- (void)setToolbarPosition:(ToolbarPosition)position
                   passKey:(LayoutStateToolbarPassKey)passKey {
  if (_toolbarPosition == position) {
    return;
  }
  _toolbarPosition = position;
  [_observers layoutState:self didChangeToolbarPosition:position];
}

- (void)setAppBarPosition:(AppBarPosition)position
                  passKey:(LayoutStateScenePassKey)passKey {
  if (_appBarPosition == position) {
    return;
  }
  _appBarPosition = position;
  [_observers layoutState:self didChangeAppBarPosition:position];
}

- (void)setContainedLayoutActive:(BOOL)active
       withTransitionCoordinator:(id<LayoutTransitionCoordinating>)coordinator
                    scenePassKey:(LayoutStateScenePassKey)passKey {
  if (_containedLayoutActive == active) {
    return;
  }

  [_observers layoutState:self
      willChangeContainedLayout:active
      withTransitionCoordinator:coordinator];

  _containedLayoutActive = active;
}

- (void)setContainedLayoutActive:(BOOL)active
       withTransitionCoordinator:(id<LayoutTransitionCoordinating>)coordinator
                assistantPassKey:(LayoutStateAssistantPassKey)passKey {
  if (_containedLayoutActive == active) {
    return;
  }

  [_observers layoutState:self
      willChangeContainedLayout:active
      withTransitionCoordinator:coordinator];

  _containedLayoutActive = active;
}

#pragma mark - Public

- (void)updateAppBarPositionWithView:(UIView*)view
                         coordinator:(id<UIViewControllerTransitionCoordinator>)
                                         coordinator
                             passKey:(LayoutStateScenePassKey)passKey {
  AppBarPosition position = [self calculateAppBarPositionWithView:view
                                                      coordinator:coordinator];
  if (_appBarPosition == position) {
    return;
  }
  _appBarPosition = position;
  [_observers layoutState:self didChangeAppBarPosition:position];
}

- (void)addObserver:(id<LayoutStateObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<LayoutStateObserver>)observer {
  [_observers removeObserver:observer];
}

#pragma mark - Private

// Calculates the App Bar position based on the view and coordinator.
- (AppBarPosition)
    calculateAppBarPositionWithView:(UIView*)view
                        coordinator:(id<UIViewControllerTransitionCoordinator>)
                                        coordinator {
  UIWindowScene* scene = view.window.windowScene;
  if (!scene) {
    return AppBarPosition::kNone;
  }

  if (IsRegularXRegularSizeClass(view)) {
    return AppBarPosition::kNone;
  }

  if (!IsCompactHeight(view.traitCollection)) {
    return AppBarPosition::kBottom;
  }

  UIInterfaceOrientation orientation =
      scene.effectiveGeometry.interfaceOrientation;

  if (@available(iOS 26, *)) {
    // effectiveGeometry is already reliable on iOS 26.
  } else {
    if (coordinator) {
      orientation = GetTargetInterfaceOrientation(orientation,
                                                  coordinator.targetTransform);
    }
  }

  switch (orientation) {
    case UIInterfaceOrientationPortrait:
      return AppBarPosition::kBottom;
    case UIInterfaceOrientationLandscapeLeft:
      return AppBarPosition::kLeft;
    case UIInterfaceOrientationLandscapeRight:
      return AppBarPosition::kRight;
    default:
      return AppBarPosition::kNone;
  }
}

@end
