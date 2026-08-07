// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/public/scoped_force_fullscreen.h"

#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test handler implementing FullscreenCommands to verify ScopedForceFullscreen.
@interface TestFullscreenCommandsHandler : NSObject <FullscreenCommands>
@property(nonatomic, assign) BOOL forceFullscreenEnabled;
@property(nonatomic, assign) ForceFullscreenFeature lastFeature;
@property(nonatomic, assign) BOOL respondToForceSelector;
@end

@implementation TestFullscreenCommandsHandler

- (instancetype)init {
  if ((self = [super init])) {
    _respondToForceSelector = YES;
  }
  return self;
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  if (aSelector == @selector(forceFullscreen:feature:) &&
      !self.respondToForceSelector) {
    return NO;
  }
  return [super respondsToSelector:aSelector];
}

- (void)enterFullscreenWithTrigger:(FullscreenModeTransitionTrigger)trigger
                          animated:(BOOL)animated {
}
- (void)exitFullscreenWithTrigger:(FullscreenModeTransitionTrigger)trigger
                         animated:(BOOL)animated {
}
- (void)disableFullscreenAnimated:(BOOL)animated {
}
- (void)reenableFullscreen {
}
- (void)exitForceFullscreen {
}

- (void)forceFullscreen:(BOOL)enable feature:(ForceFullscreenFeature)feature {
  self.forceFullscreenEnabled = enable;
  self.lastFeature = feature;
}

@end

using ScopedForceFullscreenTest = PlatformTest;

// Test that ScopedForceFullscreen enables and disables forced fullscreen mode.
TEST_F(ScopedForceFullscreenTest, Lifecycle) {
  TestFullscreenCommandsHandler* handler =
      [[TestFullscreenCommandsHandler alloc] init];
  EXPECT_FALSE(handler.forceFullscreenEnabled);

  {
    ScopedForceFullscreen scoped_force(handler,
                                       ForceFullscreenFeature::kHideToolbars);
    EXPECT_TRUE(handler.forceFullscreenEnabled);
    EXPECT_EQ(ForceFullscreenFeature::kHideToolbars, handler.lastFeature);
  }

  EXPECT_FALSE(handler.forceFullscreenEnabled);
  EXPECT_EQ(ForceFullscreenFeature::kHideToolbars, handler.lastFeature);
}

// Test that ScopedForceFullscreen does not crash if handler no longer responds
// during teardown.
TEST_F(ScopedForceFullscreenTest, SafeTeardown) {
  TestFullscreenCommandsHandler* handler =
      [[TestFullscreenCommandsHandler alloc] init];

  {
    ScopedForceFullscreen scoped_force(handler,
                                       ForceFullscreenFeature::kFindInPage);
    EXPECT_TRUE(handler.forceFullscreenEnabled);
    handler.respondToForceSelector = NO;
  }

  // Destructor should not have invoked forceFullscreen:NO since selector
  // response was disabled.
  EXPECT_TRUE(handler.forceFullscreenEnabled);
}
