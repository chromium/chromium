// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Interop
import UIKit
import XCTest

class UniquePointerTest: XCTestCase {

  func testReturnedObjectPointer() throws {
    var returner = ValueReturner()
    let object = returner.Object()
    XCTAssertEqual(object.pointee.IsValid(), true, "")
    XCTAssertEqual(object.pointee.GetValue(), 42, "")
  }

}
