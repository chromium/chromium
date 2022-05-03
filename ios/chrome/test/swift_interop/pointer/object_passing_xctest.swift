// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Pointer
import XCTest

class ObjectPassingTest: XCTestCase {

  func testReferenceParameters() throws {
    #if swift(>=5.7)
      let a = Object(10)
      let b = Object(20)
    #else
      var a = Object(10)
      var b = Object(20)
    #endif
    #if swift(>=5.6)
      let passer = ObjectPassing()
    #else
      var passer = ObjectPassing()
    #endif
    XCTAssertEqual(a.GetValue(), 10)
    #if swift(>=5.7)
      XCTAssertEqual(passer.AddReferences(a, b), 30)
    #else
      XCTAssertEqual(passer.AddReferences(&a, &b), 30)
    #endif
  }

  func testPointerParameters() throws {
    var a = Object(10)
    var b = Object(20)
    #if swift(>=5.6)
      let passer = ObjectPassing()
    #else
      var passer = ObjectPassing()
    #endif

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
  #if swift(>=5.6)
    func addObjects(one: Object, two: Object) -> Int32 {
      return one.GetValue() + two.GetValue()
    }
  #endif

  func addPointerObjects(one: UnsafeMutablePointer<Object>, two: UnsafeMutablePointer<Object>)
    -> Int32
  {
    return one.pointee.GetValue() + two.pointee.GetValue()
  }

  func testPassToSwift() throws {
    var a = Object(10)
    var b = Object(20)

    #if swift(>=5.6)
      XCTAssertEqual(addObjects(one: a, two: b), 30)
    #endif
    XCTAssertEqual(addPointerObjects(one: &a, two: &b), 30)
  }
}
