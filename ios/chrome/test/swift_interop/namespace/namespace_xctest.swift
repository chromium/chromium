// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Namespace
import UIKit
import XCTest

class NamespaceTest: XCTestCase {

  func testNamespaceClass() throws {
    // Non-namespaced class.
    let goat = Goat()
    XCTAssertEqual(goat.GetValue(), 7, "Values don't match")

    // Namespaced class with the same type name, verify the namespaced one
    // is the one created.
    let spaceGoat = space.Goat()
    spaceGoat.DoNothing()
    XCTAssertEqual(spaceGoat.GetValue(), 42, "Values don't match")
  }

  func testNamespaceTypedEnum_noCompile() throws {
    // namespaced typed enum.
    // DOESN'T COMPILE: 'Vehicle' has no member 'boat' (it does).

    // let vehicle = space.Vehicle.boat
  }

  func testNamespaceClassEnum() throws {
    // Use a namespaced class enum.
    // Compiles ONLY with Swift greater than version 5.6 (Xcode 13.3).
    let animal = space.Animal.goat
    XCTAssertEqual(animal, space.Animal.goat, "values don't match")
    XCTAssertNotEqual(animal, space.Animal.dog, "values don't match")
  }

  func testNestedNamespace() throws {
    let goat = outer.inner.NestedGoat()
    XCTAssertEqual(goat.GetValue(), 50, "values don't match")
  }

  func testClassEnumInOutNamespace_crashes() throws {
    // Test the enum class not in a namespace.
    let food = SameNameEnum.watermelon
    XCTAssertNotEqual(food, SameNameEnum.orange, "something broke")

    // Test the enum class in a namespace.
    // CRASHES in XCTAssertNotEqual()
    // In progress CL: https://github.com/apple/swift/pull/42494.
    // let moreFood = sameName.SameNameEnum.watermelon;
    // XCTAssertNotEqual(moreFood, sameName.SameNameEnum.orange, "something broke")
  }
}
