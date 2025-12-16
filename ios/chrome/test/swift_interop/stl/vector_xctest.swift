// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Interop
import XCTest

final class VectorTests: XCTestCase {

  func testVectorReturnValue() {
    let array = GetFortyTwoVector()
    XCTAssertEqual(array.size(), 1)
    XCTAssertEqual(array[0], 42)
    // TODO(crbug.com/465131182):
    // Conformance of std::vector to the RandomAccessCollection protocol does
    // not currently work. We probably need to get `import std` to work to
    // solve this, which would require building it against the Chromium
    // toolchain's version of libc++ to resolve link-time ODR violations.
  }

  func testVectorArgument() {
    var testVector = IntVector()
    testVector.push_back(42)
    XCTAssertTrue(CheckFortyTwoInVector(testVector))
  }

}
