// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Language
import XCTest

class NoDiscardTest: XCTestCase {

  func testNoDiscardFunction() throws {
    let value = NoDiscardAdd(5, 10)
    XCTAssertEqual(value, 15)

    // NoDiscardAdd() is nodiscard, corectly gives a warning.
    // NoDiscardAdd(10,10)
  }

  func testNoDiscardMethod() throws {
    var multiplier = NoDiscardMultiply()
    let value = multiplier.Multiply(10, 10)
    XCTAssertEqual(value, 100)

    // NoDiscardMultiply::Multiply is nodiscard, correctly gives a warning.
    // multiplier.Multiply(50, 50)

    let result = multiplier.Divide(100, 10)
    XCTAssertEqual(result, 10)

    // NoDiscardMultiply::Divide has no annotation, correctly doesn't give a warning
    multiplier.Divide(100, 10)
  }

  func testNoDiscardStruct() throws {
    let error: NoDiscardError = NoDiscardReturnError(10, 10)
    XCTAssertEqual(error.value_, 20)

    // NoDiscardError is declared nodiscard, so ignoring the return value of
    // NoDiscardReturnError() should be a warning, but isn't.
    // Filed as: https://github.com/apple/swift/issues/59002
    NoDiscardReturnError(50, 50)
  }
}
