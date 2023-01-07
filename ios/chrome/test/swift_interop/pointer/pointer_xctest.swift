// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Pointer
import UIKit
import XCTest

class PointerTest: XCTestCase {

  func testReturnedPointer() throws {
    var returner = PointerReturner()
    XCTAssertEqual(returner.Valid(), true, "Object not valid?")
    let intPtr = returner.IntegerPointer()!
    XCTAssertEqual(intPtr.pointee, 17, "Pointer-to-integer return broken")
  }

  func testReturnedObjectPointer() throws {
    var returner = PointerReturner()
    let ptr = returner.ObjectPointer()!
    XCTAssertEqual(ptr.pointee.Valid(), true, "Object returned by pointer not valid?")
    let intPtr = ptr.pointee.IntegerPointer()!
    XCTAssertEqual(intPtr.pointee, 17, "Pointer-to-integer returned from pointer-to-object broken")
  }

}
