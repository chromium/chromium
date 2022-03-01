// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import UIKit
import XCTest

import Namespace

class NamespaceTest: XCTestCase {

  func testNamespaceClass() throws {
    // Non-namespaced class.
    var goat = Goat()
    XCTAssertEqual(goat.GetValue(), 7, "Values don't match")

    // Namespaced class with the same type name, verify the namespaced one
    // is the one created.
    var spaceGoat = space.Goat()
    spaceGoat.DoNothing()
    XCTAssertEqual(spaceGoat.GetValue(), 42, "Values don't match")
  }

  // These either fail to compile generate asserts.
  // Note: These work in ToT swiftc.
  func testNamespaceEnum() throws {
    // namespaced typed enum.
    // DOESN'T COMPILE: 'Vehicle' has no member 'boat' (it does).

    // let vehicle = space.Vehicle.boat

    // namespaced class enum.
    // ASSERTS:
    // While evaluating request ASTLoweringRequest(Lowering AST to SIL for
    // module ios_chrome_test_swift_interop_swift_interop_tests)
    // top-level value not found
    // Cross-reference to module '__ObjC'
    // ... space

    // let animal = space.Animal.goat
    // XCTAssertEqual(animal, space.Animal.goat, "values don't match")
    // XCTAssertNotEqual(animal, space.Animal.dog, "values don't match")
  }

  func testNestedNamespace() throws {
    var goat = outer.inner.NestedGoat();
    XCTAssertEqual(goat.GetValue(), 50, "values don't match")
  }
}

