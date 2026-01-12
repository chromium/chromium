// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "base/logging.h"
#import "ios/chrome/test/swift_interop/objc/swift_api.h"

@interface ObjCInteropTestCase : XCTestCase
@end

@implementation ObjCInteropTestCase

- (void)testEmpty {
  LOG(INFO) << "This is a dependency on //base";
}

- (void)testSwiftString {
  swift::String string = "string";
  swift::String updatedString = SwiftAPI::updateSwiftString(string);

  XCTAssertFalse(updatedString.isEmpty());
  XCTAssertEqual(14, updatedString.getCount());
  XCTAssertEqual("string updated", (std::string)updatedString);
}

- (void)testSwiftArray {
  auto array = swift::Array<swift::Int>::init();
  array.append(0);
  XCTAssertEqual(1, array.getCount());

  swift::Array<swift::Int> updatedArray = SwiftAPI::updateSwiftArray(array);
  XCTAssertEqual(2, array.getCount());
  XCTAssertEqual(0, updatedArray[0]);
  XCTAssertEqual(42, updatedArray[1]);
}

- (void)testSwiftOptional {
  {
    swift::Optional<swift::String> string =
        SwiftAPI::createOptionalString(true);
    XCTAssertFalse(string.get().isEmpty());
    XCTAssertEqual(14, string.get().getCount());
    XCTAssertEqual("string created", (std::string)string.get());
  }

  {
    swift::Optional<swift::String> string =
        SwiftAPI::createOptionalString(false);
    XCTAssertFalse(string);
  }
}

- (void)testSwiftStruct {
  SwiftAPI::TestStruct obj = SwiftAPI::TestStruct::init("name", 1);
  XCTAssertEqual(1, obj.getNumber());
  XCTAssertEqual("name", (std::string)obj.getName());

  obj.incrementNumber();
  obj.updateName("new name");

  XCTAssertEqual(2, obj.getNumber());
  XCTAssertEqual("new name", (std::string)obj.getName());
}

@end
