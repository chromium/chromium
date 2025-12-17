// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "base/logging.h"
#import "ios/chrome/test/swift_interop/swift_api.h"
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

- (void)testSwiftString {
  swift::String string = "string";
  swift::String updatedString =
      ios_chrome_test_swift_interop_swift_api::updateSwiftString(string);

  XCTAssertFalse(updatedString.isEmpty());
  XCTAssertEqual(14, updatedString.getCount());
  XCTAssertEqual("string updated", (std::string)updatedString);
}

- (void)testSwiftArray {
  auto array = swift::Array<swift::Int>::init();
  array.append(0);
  XCTAssertEqual(1, array.getCount());

  swift::Array<swift::Int> updatedArray =
      ios_chrome_test_swift_interop_swift_api::updateSwiftArray(array);
  XCTAssertEqual(2, array.getCount());
  XCTAssertEqual(0, updatedArray[0]);
  XCTAssertEqual(42, updatedArray[1]);
}

- (void)testSwiftOptional {
  {
    swift::Optional<swift::String> string =
        ios_chrome_test_swift_interop_swift_api::createOptionalString(true);
    XCTAssertFalse(string.get().isEmpty());
    XCTAssertEqual(14, string.get().getCount());
    XCTAssertEqual("string created", (std::string)string.get());
  }

  {
    swift::Optional<swift::String> string =
        ios_chrome_test_swift_interop_swift_api::createOptionalString(false);
    XCTAssertFalse(string);
  }
}

- (void)testSwiftStruct {
  ios_chrome_test_swift_interop_swift_api::TestStruct obj =
      ios_chrome_test_swift_interop_swift_api::TestStruct::init("name", 1);
  XCTAssertEqual(1, obj.getNumber());
  XCTAssertEqual("name", (std::string)obj.getName());

  obj.incrementNumber();
  obj.updateName("new name");

  XCTAssertEqual(2, obj.getNumber());
  XCTAssertEqual("new name", (std::string)obj.getName());
}

@end
