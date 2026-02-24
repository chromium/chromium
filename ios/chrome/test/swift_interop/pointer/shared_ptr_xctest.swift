// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import CxxImports
import XCTest

class SharedPointerTest: XCTestCase {

  override func setUp() {
    super.setUp()
    ResetTotalReferenceCount()
    XCTAssertEqual(GetTotalReferenceCount(), 0, "Counter did not reset")
  }

  override func tearDown() {
    XCTAssertEqual(GetTotalReferenceCount(), 0, "C++ object is leaked")
    super.tearDown()
  }

  func testSharedObject() throws {
    let object = SharedObject.MakeForSwift(42)
    XCTAssertEqual(object!.IsValid(), true)
    XCTAssertEqual(object!.GetValue(), 42)
    XCTAssertEqual(GetTotalReferenceCount(), 1)
  }

  func testTwoSharedObjects() throws {
    let object1 = SharedObject.MakeForSwift(1)
    XCTAssertEqual(object1!.IsValid(), true)
    XCTAssertEqual(object1!.GetValue(), 1)
    XCTAssertEqual(GetTotalReferenceCount(), 1)

    let object2 = SharedObject.MakeForSwift(2)
    XCTAssertEqual(object2!.IsValid(), true)
    XCTAssertEqual(object2!.GetValue(), 2)
    XCTAssertEqual(GetTotalReferenceCount(), 2)
  }

  func testLifetimeManagedByARC() throws {
    var object: SharedObject? = SharedObject.MakeForSwift(42)
    XCTAssertEqual(object!.IsValid(), true)
    XCTAssertEqual(object!.GetValue(), 42)
    XCTAssertEqual(GetTotalReferenceCount(), 1)

    var object2: SharedObject? = object
    XCTAssertEqual(object2!.IsValid(), true)
    XCTAssertEqual(object2!.GetValue(), 42)
    XCTAssertEqual(GetTotalReferenceCount(), 2)

    var object3: SharedObject? = object
    XCTAssertEqual(object3!.IsValid(), true)
    XCTAssertEqual(object3!.GetValue(), 42)
    XCTAssertEqual(GetTotalReferenceCount(), 3)

    object = nil
    XCTAssertEqual(GetTotalReferenceCount(), 2)

    object2 = nil
    XCTAssertEqual(GetTotalReferenceCount(), 1)

    object3 = nil
    XCTAssertEqual(GetTotalReferenceCount(), 0)
  }

  func testLifetimeByAutoreleasePool() {
    autoreleasepool {
      let object = SharedObject.MakeForSwift(42)
      XCTAssertEqual(object!.IsValid(), true)
      XCTAssertEqual(object!.GetValue(), 42)
      XCTAssertEqual(GetTotalReferenceCount(), 1)
    }
    XCTAssertEqual(GetTotalReferenceCount(), 0)
  }

}
