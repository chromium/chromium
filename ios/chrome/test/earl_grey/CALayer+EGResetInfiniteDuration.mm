// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

namespace {

// Swizzles the given methods in the given class.
static void SwizzleInstanceMethod(Class cls,
                                  SEL original_selector,
                                  SEL swizzled_selector) {
  Method original_method = class_getInstanceMethod(cls, original_selector);
  Method swizzled_method = class_getInstanceMethod(cls, swizzled_selector);

  BOOL did_add_method = class_addMethod(
      cls, original_selector, method_getImplementation(swizzled_method),
      method_getTypeEncoding(swizzled_method));

  if (did_add_method) {
    class_replaceMethod(cls, swizzled_selector,
                        method_getImplementation(original_method),
                        method_getTypeEncoding(original_method));
  } else {
    method_exchangeImplementations(original_method, swizzled_method);
  }
}

}  // namespace

// A swizzle category that resets the duration to zero of animations when they
// are added to the layer, if the duration was INFINITY. This ensures that
// Earl Grey does not try to track them. These animations seem to be related
// to SwiftUI.
@interface CALayer (EGResetInfiniteDuration)
@end

@implementation CALayer (EGResetInfiniteDuration)

+ (void)load {
  SwizzleInstanceMethod([self class], @selector(addAnimation:forKey:),
                        @selector(swizzledInfiniteDuration_addAnimation:
                                                                 forKey:));
}

- (void)swizzledInfiniteDuration_addAnimation:(CAAnimation*)animation
                                       forKey:(NSString*)key {
  if (animation.duration == +INFINITY) {
    // Reset duration of `CAMatchPropertyAnimation` and `CAMatchMoveAnimation`
    // from infinite to zero so that Earl Grey does not track them infinitely,
    // until a 10 second timeout is hit.
    animation.duration = 0;
  }

  // Call the original implementation.
  [self swizzledInfiniteDuration_addAnimation:animation forKey:key];
}

@end
