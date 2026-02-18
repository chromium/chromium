// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import CxxImports
import XCTest

class TimeTest: XCTestCase {

  func testSeconds() {
    let ts = timespec(tv_sec: 2, tv_nsec: 0)
    let timeDelta: base.swift.TimeDelta = base.swift.TimeDelta.FromTimeSpec(ts)
    XCTAssertEqual(2, timeDelta.InSeconds())
    XCTAssertEqual(2000, timeDelta.InMilliseconds())
    XCTAssertEqual(2_000_000, timeDelta.InMicroseconds())
    XCTAssertEqual(2_000_000_000, timeDelta.InNanoseconds())
  }

  func testMilliseconds() {
    let ts = timespec(tv_sec: 0, tv_nsec: 2_000_000)
    let timeDelta: base.swift.TimeDelta = base.swift.TimeDelta.FromTimeSpec(ts)
    XCTAssertEqual(0, timeDelta.InSeconds())
    XCTAssertEqual(2, timeDelta.InMilliseconds())
    XCTAssertEqual(2000, timeDelta.InMicroseconds())
    XCTAssertEqual(2_000_000, timeDelta.InNanoseconds())
  }

  func testMicroseconds() {
    let ts = timespec(tv_sec: 0, tv_nsec: 2000)
    let timeDelta: base.swift.TimeDelta = base.swift.TimeDelta.FromTimeSpec(ts)
    XCTAssertEqual(0, timeDelta.InSeconds())
    XCTAssertEqual(0, timeDelta.InMilliseconds())
    XCTAssertEqual(2, timeDelta.InMicroseconds())
    XCTAssertEqual(2000, timeDelta.InNanoseconds())
  }

  func testMix() {
    let ts = timespec(tv_sec: 1, tv_nsec: 20000)
    let timeDelta: base.swift.TimeDelta = base.swift.TimeDelta.FromTimeSpec(ts)
    XCTAssertEqual(1, timeDelta.InSeconds())
    XCTAssertEqual(1000, timeDelta.InMilliseconds())
    XCTAssertEqual(1_000_020, timeDelta.InMicroseconds())
    XCTAssertEqual(1_000_020_000, timeDelta.InNanoseconds())
  }

}
