// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Pointer
import UIKit
import XCTest

class UniquePointerTest: XCTestCase {

  func testReturnedObjectPointer() throws {
    var returner = ValueReturner()
    let object = returner.ObjectPointer()!
    XCTAssertEqual(object.pointee.IsValid(), true, "")
    XCTAssertEqual(object.pointee.GetValue(), 17, "")
  }

}
