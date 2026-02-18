// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import CxxImports
import XCTest

class SysInfoTest: XCTestCase {

  func testIOSBuildNumber() {
    let cxxBuildNumber = base.swift.GetIOSBuildNumber()
    let buildNumber = String(cString: cxxBuildNumber.__c_strUnsafe())
    XCTAssertNotNil(buildNumber)
    XCTAssertNotEqual(buildNumber, "")
  }

}
