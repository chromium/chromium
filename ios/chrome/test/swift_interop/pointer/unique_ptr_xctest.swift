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

    // DOESN'T COMPILE: value of type 'Value' has no member 'GetValue'
    // in 5.7 official builds. http://crbug.com/1336937
    #if swift(<5.7)
      XCTAssertEqual(object.pointee.GetValue(), 17, "")
    #endif
  }

}
