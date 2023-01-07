// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Classes
import UIKit
import XCTest

class InlinedClassTest: XCTestCase {

  func testInlined() throws {
    var obj = InlinedClass()
    var num: Int32 = obj.AddTo(10)
    XCTAssertEqual(num, 10, "Addition didn't work correctly")
    num = obj.AddTo(5)
    XCTAssertEqual(num, 15, "Addition didn't work correctly")

    var composed = ComposedClass()
    composed.Increment(2)
    let result: Int32 = composed.Increment(8)
    XCTAssertEqual(result, 10, "Stack based class didn't work")
  }

}
