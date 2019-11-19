// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/snackbar/snackbar_coordinator.h"

#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using SnackbarCoordinatorTest = PlatformTest;

// Tests that the coordinator handles snackbar commands.
TEST_F(SnackbarCoordinatorTest, RegistersDispatching) {
  id dispatcher = OCMClassMock([CommandDispatcher class]);
  UIViewController* baseViewController = [[UIViewController alloc] init];
  SnackbarCoordinator* coordinator = [[SnackbarCoordinator alloc]
      initWithBaseViewController:baseViewController];
  coordinator.dispatcher = dispatcher;
  [[dispatcher expect] startDispatchingToTarget:coordinator
                                    forProtocol:@protocol(SnackbarCommands)];
  [coordinator start];
  EXPECT_OCMOCK_VERIFY(dispatcher);
  EXPECT_TRUE([coordinator respondsToSelector:@selector(showSnackbarMessage:)]);
  EXPECT_TRUE([coordinator respondsToSelector:@selector(showSnackbarMessage:
                                                               bottomOffset:)]);
}
