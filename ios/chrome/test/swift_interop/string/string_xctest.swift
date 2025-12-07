// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Interop
import UIKit
import XCTest

// TODO(crbug.com/456484598): Move this extension to a public module that can be used in an app.
// Now having both `import CxxStdlibShim (or CxxStdlib)` and `import Interop` causes a linker error
/// like "error: 'std::XXX' from module 'Interop' is not present in definition of 'std::XXX' in
// module 'CxxStdlibShim'".
// In this test file, XCTest indirectly imports the standard library like `std.string` so we don't
// need to import CxxStdlibShim (or CxxStdlib).
extension String {
  var cxxString: std.string {
    let length = self.utf8.count
    return self.withCString { charPointer in
      return std.string(charPointer, std.size_t(length))
    }
  }

  init(_ cxxString: std.string) {
    self.init(cString: cxxString.__c_strUnsafe())
  }
}

class StringTest: XCTestCase {

  func testString() throws {
    // Convert a Swift string to a C++ string.
    let swiftString = "Test"
    let cxxString: std.string = swiftString.cxxString

    // Modify the string in a C++ function.
    let newString = addStringFromCxx(cxxString)

    // Convert a C++ string to a Swift string.
    let swiftNewString = String(newString)
    XCTAssertEqual(swiftNewString, "Test string added in C++!")
  }

}
