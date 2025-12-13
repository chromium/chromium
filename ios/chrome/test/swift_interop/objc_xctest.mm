// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "base/logging.h"
#import "ios/chrome/test/swift_interop/swift_interop_tests.h"

@interface ObjCInteropTestCase : XCTestCase
@end

@implementation ObjCInteropTestCase

- (void)testEmpty {
  LOG(INFO) << "This is a dependency on //base";
}

- (void)testCallSwiftFromObjC {
  // Verify that Objective-C can call swift code that was compiled with
  // C++ interop enabled.
  EnumTest* test = [[EnumTest alloc] init];
  NSError* error = nil;
  BOOL success = [test testEnumsAndReturnError:&error];
  XCTAssertTrue(success);
  XCTAssertNil(error, @"Error: %@", error);
}

@end
