// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Pointer
import XCTest

class ObjectPassingTest: XCTestCase {

  func testReferenceParameters() throws {
    let a = Object(10)
    let b = Object(20)
    let passer = ObjectPassing()
    XCTAssertEqual(a.GetValue(), 10)
    XCTAssertEqual(passer.AddReferences(a, b), 30)
  }

  func testPointerParameters() throws {
    var a = Object(10)
    var b = Object(20)
    let passer = ObjectPassing()

    XCTAssertEqual(a.GetValue(), 10)
    XCTAssertEqual(passer.AddPointers(&a, &b), 30)
  }

  func testMismatchedParameters() throws {
    // var obj = Object(10)
    // var str = "Hello"
    // let passer = ObjectPassing()

    // Correctly doesn't compile because the types don't match. Error is
    // "cannot convert value of type 'UnsafeMutablePointer<String>' to expected
    // argument type 'UnsafeMutablePointer<Object>'.
    // XCTAssertEqual(passer.AddPointers(&obj, &obj, &str), 60, "")
  }

  // Note: if this method returns `Int` instead of `Int32` the compiler
  // will crash (filed as https://github.com/apple/swift/issues/58458).
  // This has been fixed in ToT.
  // Note: prior to Swift 5.6, calling GetValue() (which is const) on these objects
  // results in a compiler error about calling immutable methods on a `let` object.
  // Omit this test on earlier Swift versions.
  func addObjects(one: Object, two: Object) -> Int32 {
    return one.GetValue() + two.GetValue()
  }

  func addPointerObjects(one: UnsafeMutablePointer<Object>, two: UnsafeMutablePointer<Object>)
    -> Int32
  {
    return one.pointee.GetValue() + two.pointee.GetValue()
  }

  func testPassToSwift() throws {
    var a = Object(10)
    var b = Object(20)

    XCTAssertEqual(addObjects(one: a, two: b), 30)
    XCTAssertEqual(addPointerObjects(one: &a, two: &b), 30)
  }
}
