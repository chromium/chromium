// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Base
import XCTest

class SysInfoTest: XCTestCase {

  func testIOSBuildNumber() {
    let cxx_build_number = base.SysInfo.GetIOSBuildNumber()
    let build_number = String(cString: cxx_build_number.__c_strUnsafe())
    XCTAssertNotNil(build_number)
    XCTAssertNotEqual(build_number, "")
  }

}
