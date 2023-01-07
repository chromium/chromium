// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/ocmock/OCMockObject+BreakpadControllerTesting.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OCMockObject (BreakpadControllerTesting)

- (void)cr_expectGetCrashReportCount:(int)crashReportCount {
  id invocationBlock = ^(NSInvocation* invocation) {
    __unsafe_unretained void (^block)(int);
    [invocation getArgument:&block atIndex:2];
    if (!block) {
      ADD_FAILURE();
      return;
    }
    block(crashReportCount);
  };
  BreakpadController* breakpadController =
      static_cast<BreakpadController*>([[self expect] andDo:invocationBlock]);
  [breakpadController getCrashReportCount:[OCMArg isNotNil]];
}

@end
