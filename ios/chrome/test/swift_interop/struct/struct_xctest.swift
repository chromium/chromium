// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Struct
import XCTest

class StructTest: XCTestCase {

  func testStruct() throws {
    var goodFoo = Foolean()
    var badFoo = Foolean()

    // Test calling static method from struct.
    goodFoo.value = true
    goodFoo.description = Foolean.GetDescriptionForValue(true)
    badFoo.value = false
    badFoo.description = Foolean.GetDescriptionForValue(true)

    // Test passing object defined in C++ and initialized in Swift to top level C++ functions.
    XCTAssertTrue(IsFooleanValid(goodFoo))
    XCTAssertFalse(IsFooleanValid(badFoo))
  }

}
