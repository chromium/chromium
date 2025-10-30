// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Interop
import UIKit
import XCTest

class SharedPointerTest: XCTestCase {

  override func setUp() {
    super.setUp()
    ResetSharedObjectLiveCount()
    XCTAssertEqual(GetSharedObjectLiveCount(), 0, "Counter did not reset")
  }

  override func tearDown() {
    XCTAssertEqual(GetSharedObjectLiveCount(), 0, "C++ object is leaked")
    super.tearDown()
  }

  func testSharedObject() throws {
    let object = SharedObject.create(42)
    XCTAssertEqual(object!.IsValid(), true, "Object should be valid")
    XCTAssertEqual(object!.GetValue(), 42, "The value is wrong")
    XCTAssertEqual(GetSharedObjectLiveCount(), 1, "The count of living objects is wrong")
  }

  func testLifetimeManagedByARC() throws {
    var object: SharedObject? = SharedObject.create(42)
    XCTAssertEqual(object!.IsValid(), true, "Object should be valid")
    XCTAssertEqual(object!.GetValue(), 42, "The value is wrong")
    XCTAssertEqual(GetSharedObjectLiveCount(), 1, "The count of living objects is wrong")

    var object2: SharedObject? = object
    XCTAssertEqual(object2!.IsValid(), true, "Object should be valid")
    XCTAssertEqual(object2!.GetValue(), 42, "The value is wrong")
    XCTAssertEqual(GetSharedObjectLiveCount(), 2, "The count of living objects is wrong")

    var object3: SharedObject? = object
    XCTAssertEqual(object3!.IsValid(), true, "Object should be valid")
    XCTAssertEqual(object3!.GetValue(), 42, "The value is wrong")
    XCTAssertEqual(GetSharedObjectLiveCount(), 3, "The count of living objects is wrong")

    object = nil
    XCTAssertEqual(GetSharedObjectLiveCount(), 2, "The count of living objects is wrong")

    object2 = nil
    XCTAssertEqual(GetSharedObjectLiveCount(), 1, "The count of living objects is wrong")

    object3 = nil
    XCTAssertEqual(GetSharedObjectLiveCount(), 0, "The count of living objects is wrong")
  }

  func testLifetimeByAutoreleasePool() {
    autoreleasepool {
      let object = SharedObject.create(42)
      XCTAssertEqual(object!.IsValid(), true, "Object should be valid")
      XCTAssertEqual(object!.GetValue(), 42, "The value is wrong")
      XCTAssertEqual(GetSharedObjectLiveCount(), 1, "The count of living objects is wrong")
    }
    XCTAssertEqual(GetSharedObjectLiveCount(), 0, "The count of living objects is wrong")
  }

}
