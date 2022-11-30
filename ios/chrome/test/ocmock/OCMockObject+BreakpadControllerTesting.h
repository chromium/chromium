// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_OCMOCK_OCMOCKOBJECT_BREAKPADCONTROLLERTESTING_H_
#define IOS_CHROME_TEST_OCMOCK_OCMOCKOBJECT_BREAKPADCONTROLLERTESTING_H_

#import "third_party/ocmock/OCMock/OCMockObject.h"

#import <Foundation/Foundation.h>

// Private methods for unit tests.
@interface OCMockObject (BreakpadControllerTesting)

// Sets an expectation for invoking -[BreakpadController getCrashReportCount:]
// with a non-nil result block. Arranges to pass `crashReportCount` to the
// result block when the expectation is met.
- (void)cr_expectGetCrashReportCount:(int)crashReportCount;

@end

#endif  // IOS_CHROME_TEST_OCMOCK_OCMOCKOBJECT_BREAKPADCONTROLLERTESTING_H_
