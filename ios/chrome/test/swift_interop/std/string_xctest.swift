// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import ChromiumCxxStdlib
import CxxImports
import XCTest

class StringTest: XCTestCase {
  func testString() throws {
    XCTAssertEqual(addStringFromCxx("Test"), "Test string added in C++!")
  }
}
