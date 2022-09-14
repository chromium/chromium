// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#import <ostream>

#import "base/check_op.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns a pointer style with a hover effect with a slight tint and no pointer
// shape (i.e., the pointer stays the same).
UIPointerStyle* CreateHoverEffectNoShapePointerStyle(UIButton* button)
    API_AVAILABLE(ios(13.4)) {
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
UIPointerStyle* CreateHighlightEffectRectShapePointerStyle(UIButton* button)
    API_AVAILABLE(ios(13.4)) {
  UITargetedPreview* preview = [[UITargetedPreview alloc] initWithView:button];
  UIPointerHighlightEffect* effect =
      [UIPointerHighlightEffect effectWithPreview:preview];
  UIPointerShape* shape = [UIPointerShape shapeWithRoundedRect:button.frame];
  return [UIPointerStyle styleWithEffect:effect shape:shape];
}
}  // namespace

UIButtonPointerStyleProvider CreateDefaultEffectCirclePointerStyleProvider()
    API_AVAILABLE(ios(13.4)) {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    DCHECK_EQ(button.frame.size.width, button.frame.size.height)
        << "Pointer shape cannot be a circle since button is not square";
    UIPointerShape* shape =
        [UIPointerShape shapeWithRoundedRect:button.frame
                                cornerRadius:button.frame.size.width / 2];
    return [UIPointerStyle styleWithEffect:proposedEffect shape:shape];
  };
}

UIButtonPointerStyleProvider CreateLiftEffectCirclePointerStyleProvider()
    API_AVAILABLE(ios(13.4)) {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    DCHECK_EQ(button.frame.size.width, button.frame.size.height)
        << "Pointer shape cannot be a circle since button is not square";
    UITargetedPreview* preview =
        [[UITargetedPreview alloc] initWithView:button];
    UIPointerLiftEffect* effect =
        [UIPointerLiftEffect effectWithPreview:preview];
    UIPointerShape* shape =
        [UIPointerShape shapeWithRoundedRect:button.frame
                                cornerRadius:button.frame.size.width / 2];
    return [UIPointerStyle styleWithEffect:effect shape:shape];
  };
}

UIButtonPointerStyleProvider CreateOpaqueButtonPointerStyleProvider()
    API_AVAILABLE(ios(13.4)) {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    DCHECK(button.backgroundColor &&
           button.backgroundColor != [UIColor clearColor])
        << "Expected an opaque background for button.";
    return CreateHoverEffectNoShapePointerStyle(button);
  };
}

UIButtonPointerStyleProvider CreateTransparentButtonPointerStyleProvider()
    API_AVAILABLE(ios(13.4)) {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    DCHECK(!button.backgroundColor ||
           button.backgroundColor == [UIColor clearColor])
        << "Expected a transparent background for button.";
    return CreateHighlightEffectRectShapePointerStyle(button);
  };
}

UIButtonPointerStyleProvider
CreateOpaqueOrTransparentButtonPointerStyleProvider() API_AVAILABLE(ios(13.4)) {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    if (button.backgroundColor &&
        button.backgroundColor != [UIColor clearColor]) {
      return CreateHoverEffectNoShapePointerStyle(button);
    }
    return CreateHighlightEffectRectShapePointerStyle(button);
  };
}

API_AVAILABLE(ios(13.4))
@interface ViewPointerInteraction ()
@property(nonatomic, strong) UIPointerInteraction* pointerInteraction;
@end

API_AVAILABLE(ios(13.4))
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
                         defaultRegion:(UIPointerRegion*)defaultRegion
    API_AVAILABLE(ios(13.4)) {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region
    API_AVAILABLE(ios(13.4)) {
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
