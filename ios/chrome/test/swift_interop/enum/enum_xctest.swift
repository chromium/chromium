// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Enums
import UIKit
import XCTest

class EnumTest: XCTestCase {

  func testEnums() throws {
    // Explicitly typed.
    var color: Color = kBlue
    color = kYellow
    XCTAssertEqual(color, kYellow, "Typed enum doesn't match")

    // Implicit type. |x| is an |Int|.
    let x = kThree
    XCTAssertEqual(x, kThree, "Implicitly typed enum doesn't match")

    // Implicit type, |anotherCOlor| is a |Color|.
    let anotherColor = kYellow
    XCTAssertEqual(anotherColor, kYellow, "")
    XCTAssertNotEqual(anotherColor, kBlue)

    // These correctly fail. Cannot convert |Int| to |Color|.
    // anotherColor = kTwo
    // anotherColor = x
    // let integer : Int = kBlue

  }

  func testClassEnum() throws {
    let pet: Pet = Pet.goat
    XCTAssertEqual(pet, Pet.goat, "")
    XCTAssertNotEqual(pet, Pet.dogcow, "")
    XCTAssertNotEqual(Pet.goat, Pet.dogcow)

    // These correctly fail. Cannot convert |Int| <-> |Pet|.
    // let animal : Pet = 7
    // let number : Int = Pet.goat
  }
}
