// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Classes
import XCTest

class PolymorphismTest: XCTestCase {

  func testBasicInheritance() throws {
    var rect = Rectangle(10, 10)
    XCTAssertEqual(rect.Area(), 100)
    XCTAssertEqual(rect.NumberOfSides(), 4)

    var square = Square(5)
    XCTAssertEqual(square.Area(), 25)
    XCTAssertEqual(square.NumberOfSides(), 4)

    var triangle = Triangle(10, 10)
    XCTAssertEqual(triangle.Area(), 50)
    XCTAssertEqual(triangle.NumberOfSides(), 3)
  }

  func testInheritedMethods_noCompile() throws {
    // Test calling a public method defined in the public base class.
    // DOESN'T COMPILE: value of type 'Rectangle' has no member 'HasSides'
    // XCTAssertTrue(rect.HasSides())
  }

  func testRuntimePolymorphism_fails() throws {
    // MakeShape() creates a Triangle and returns the object as a Shape*.
    // let shape = MakeShape(10, 10)

    // DOES NOT WORK: executes the Shape implementations of these methods, not
    // the Triangle versions, so both return 0 and thus fail the comparision.
    // XCTAssertEqual(shape!.pointee.Area(), 50)
    // XCTAssertEqual(shape!.pointee.NumberOfSides(), 3)
  }

}
