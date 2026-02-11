// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import CxxImports
import XCTest

// TODO(crbug.com/465131182): We manually implements subscript() to make an
// array with move-only elements accessible via array[0].
extension CxxStringVector {
  subscript(index: Int) -> String {
    let itemPtr = self.__atUnsafe(index)

    // Rebind an unique pointer `UnsafePointer<std.unique_ptr<std.string>>` to
    // a raw pointer `UnsafePointer<UnsafePointer<std.string>>` to bypass the
    // "Move-Only" consumption check.
    return itemPtr.withMemoryRebound(
      to: UnsafePointer<std.string>.self,
      capacity: 1
    ) { rawPtrPtr in
      // Read the inner pointer.
      let stringPtr = rawPtrPtr.pointee
      // Create a string based on the dereference of the raw pointer.
      return String(stringPtr.pointee)
    }
  }
}

final class VectorTests: XCTestCase {
  func testCxxVectorConvertibleToArray() {
    let vectorFromCxx = [Int32](GetFortyTwoVector())
    XCTAssertEqual(vectorFromCxx, [42])
  }

  func testCxxVectorIterable() {
    var count = 0
    for element in GetFortyTwoVector() {
      XCTAssertEqual(element, 42)
      count += 1
    }
    XCTAssertEqual(count, 1)
  }

  func testVectorArgument() {
    let testVector: IntVector = [42]
    XCTAssertTrue(CheckFortyTwoInVector(testVector))
  }

  func testUniqueStringVector() {
    let array = GetStringVector()
    XCTAssertEqual(array[0], "a")
    XCTAssertEqual(array[1], "b")
    XCTAssertEqual(array[2], "c")
  }
}
