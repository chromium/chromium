// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Classes
import UIKit
import XCTest

class OutlinedClassTest: XCTestCase {

  func testOutlined() throws {
    var outlined = Outlined()
    XCTAssertEqual(outlined.OutlinedAddition(8), 8, "Outlined method broken")
    XCTAssertEqual(outlined.OutlinedAddition(2), 10, "Outlined method broken")
  }

  func testOutlinedInitalCtor() throws {
    var outlined = Outlined(10)
    XCTAssertEqual(outlined.OutlinedAddition(8), 18, "Outlined method broken")
    XCTAssertEqual(outlined.OutlinedAddition(2), 20, "Outlined method broken")
  }

}
