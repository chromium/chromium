// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Classes
import XCTest

class PolymorphismTest: XCTestCase {

  func testBasicInheritance() throws {
    #if swift(<5.7)
      var rect = Rectangle(10, 10)
      XCTAssertEqual(rect.Area(), 100)
      XCTAssertEqual(rect.NumberOfSides(), 4)

      var square = Square(5)
      XCTAssertEqual(square.Area(), 25)
      XCTAssertEqual(square.NumberOfSides(), 4)

      var triangle = Triangle(10, 10)
      XCTAssertEqual(triangle.Area(), 50)
      XCTAssertEqual(triangle.NumberOfSides(), 3)
    #endif
  }

  // The primary bug for inherited methods not working is
  // https://github.com/apple/swift/issues/55192. That covers several of
  // these tests.

  func testInheritedMethods_noCompile() throws {
    // Test calling a public method defined in the public base class.
    // DOESN'T COMPILE: value of type 'Rectangle' has no member 'HasSides'
    // CRASHES in 5.7-dev.
    // var rect = Rectangle(10, 10)
    // XCTAssertTrue(rect.HasSides())
  }

  func testRuntimePolymorphism_fails() throws {
    // MakeShape() creates a Triangle and returns the object as a Shape*.
    // let shape = MakeShape(10, 10)

    // DOES NOT WORK: executes the Shape implementation, not the Triangle
    // version, and thus fails the comparision (0 != 3)
    // XCTAssertEqual(shape!.pointee.NumberOfSides(), 3)
  }

  func testPureVirtualMethods_noCompile() throws {
    // MakeShape() creates a Triangle and returns the object as a Shape*.
    // let shape = MakeShape(10, 10)

    // DOES NOT COMPILE: undefined symbol Shape::Area() for lld.
    // XCTAssertEqual(shape!.pointee.Area(), 50)
  }
}
