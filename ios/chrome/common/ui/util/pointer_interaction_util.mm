// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#import <ostream>

#import "base/logging.h"

namespace {
// Returns a pointer style with a hover effect with a slight tint and no pointer
// shape (i.e., the pointer stays the same).
UIPointerStyle* CreateHoverEffectNoShapePointerStyle(UIButton* button) {
  UITargetedPreview* preview = [[UITargetedPreview alloc] initWithView:button];
  UIPointerHoverEffect* effect =
      [UIPointerHoverEffect effectWithPreview:preview];
  effect.preferredTintMode = UIPointerEffectTintModeOverlay;
  effect.prefersScaledContent = NO;
  effect.prefersShadow = NO;
  return [UIPointerStyle styleWithEffect:effect shape:nil];
}

// Returns a pointer style with a highlight effect and a rounded rectangle
// pointer shape sized to the button frame.
UIPointerStyle* CreateHighlightEffectRectShapePointerStyle(UIButton* button) {
  UITargetedPreview* preview = [[UITargetedPreview alloc] initWithView:button];
  UIPointerHighlightEffect* effect =
      [UIPointerHighlightEffect effectWithPreview:preview];
  UIPointerShape* shape = [UIPointerShape shapeWithRoundedRect:button.frame];
  return [UIPointerStyle styleWithEffect:effect shape:shape];
}
}  // namespace

UIButtonPointerStyleProvider CreateDefaultEffectCirclePointerStyleProvider() {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    CGRect frame = button.frame;
    if (abs(frame.size.width - frame.size.height) > 0.5) {
      LOG(ERROR) << "Button frame for circular pointer isn't square: ("
                 << frame.size.width << " x " << frame.size.height << ")";
    }
    UIBezierPath* path = [UIBezierPath bezierPathWithOvalInRect:frame];
    UIPointerShape* shape = [UIPointerShape shapeWithPath:path];
    return [UIPointerStyle styleWithEffect:proposedEffect shape:shape];
  };
}

UIButtonPointerStyleProvider CreateLiftEffectCirclePointerStyleProvider() {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    CGRect frame = button.frame;
    if (abs(frame.size.width - frame.size.height) > 0.5) {
      LOG(ERROR) << "Button frame for circular pointer isn't square: ("
                 << frame.size.width << " x " << frame.size.height << ")";
    }
    UITargetedPreview* preview =
        [[UITargetedPreview alloc] initWithView:button];
    UIPointerLiftEffect* effect =
        [UIPointerLiftEffect effectWithPreview:preview];
    UIBezierPath* path = [UIBezierPath bezierPathWithOvalInRect:frame];
    UIPointerShape* shape = [UIPointerShape shapeWithPath:path];
    return [UIPointerStyle styleWithEffect:effect shape:shape];
  };
}

UIButtonPointerStyleProvider CreateOpaqueButtonPointerStyleProvider() {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    if (!button.backgroundColor ||
        button.backgroundColor == [UIColor clearColor]) {
      LOG(ERROR) << "Expected an opaque background for button.";
    }
    return CreateHoverEffectNoShapePointerStyle(button);
  };
}

UIButtonPointerStyleProvider CreateTransparentButtonPointerStyleProvider() {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    if (button.backgroundColor &&
        button.backgroundColor != [UIColor clearColor]) {
      LOG(ERROR) << "Expected a transparent background for button.";
    }
    return CreateHighlightEffectRectShapePointerStyle(button);
  };
}

UIButtonPointerStyleProvider
CreateOpaqueOrTransparentButtonPointerStyleProvider() {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    if (button.backgroundColor &&
        button.backgroundColor != [UIColor clearColor]) {
      return CreateHoverEffectNoShapePointerStyle(button);
    }
    return CreateHighlightEffectRectShapePointerStyle(button);
  };
}

@interface ViewPointerInteraction ()
@property(nonatomic, strong) UIPointerInteraction* pointerInteraction;
@end

@implementation ViewPointerInteraction

- (instancetype)init {
  self = [super init];
  if (self) {
    self.pointerInteraction =
        [[UIPointerInteraction alloc] initWithDelegate:self];
  }
  return self;
}

#pragma mark UIInteraction

- (__kindof UIView*)view {
  return [self.pointerInteraction view];
}

- (void)didMoveToView:(UIView*)view {
  [self.pointerInteraction didMoveToView:view];
}

- (void)willMoveToView:(UIView*)view {
  [self.pointerInteraction willMoveToView:view];
}

#pragma mark UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region {
  if (!interaction.view.window)
    return nil;

  UIPointerHoverEffect* effect = [UIPointerHoverEffect
      effectWithPreview:[[UITargetedPreview alloc]
                            initWithView:interaction.view]];
  effect.prefersScaledContent = NO;
  effect.prefersShadow = NO;
  return [UIPointerStyle styleWithEffect:effect shape:nil];
}

@end
