// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Interop
import XCTest

class ClosureTest: XCTestCase {

  func testRepeatingClosureSingleCall() {
    let closureProvider = ClosureProvider.MakeForSwift()
    let closure: CxxRepeatingClosure = closureProvider!.GetRepeatingClosure()
    XCTAssertEqual(closureProvider!.call_count(), 0)
    closure.Run()
    XCTAssertEqual(closureProvider!.call_count(), 1)
  }

  func testRepeatingClosureMultipleCalls() {
    let closureProvider = ClosureProvider.MakeForSwift()
    let closure: CxxRepeatingClosure = closureProvider!.GetRepeatingClosure()
    XCTAssertEqual(closureProvider!.call_count(), 0)
    closure.Run()
    closure.Run()
    closure.Run()
    XCTAssertEqual(closureProvider!.call_count(), 3)
  }

  func testRepeatingClosureCallAfterUnref() {
    var closureProvider: ClosureProvider? = ClosureProvider.MakeForSwift()
    let closure: CxxRepeatingClosure = closureProvider!.GetRepeatingClosure()
    closureProvider = nil
    // Test passes by not crashing. The closure's weak binding should prevent
    // use after free.
    closure.Run()
  }

  func testOnceClosureSingleCall() {
    let closureProvider = ClosureProvider.MakeForSwift()
    let closure: CxxOnceClosure = closureProvider!.GetOnceClosure()
    XCTAssertEqual(closureProvider!.call_count(), 0)
    RunCxxOnceClosure(consuming: closure)
    XCTAssertEqual(closureProvider!.call_count(), 1)
  }
}
