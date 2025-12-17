// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

public func updateSwiftString(str: String) -> String {
  return str + " updated"
}

public func updateSwiftArray(array: inout [Int]) -> [Int] {
  array.append(42)
  return array
}

public func createOptionalString(create: Bool) -> String? {
  if create {
    return "string created"
  }
  return nil
}

public struct TestStruct {
  var name: String
  var number: Int

  public init(name: String, number: Int) {
    self.name = name
    self.number = number
  }

  public mutating func updateName(name: String) {
    self.name = name
  }

  public func getName() -> String {
    return self.name
  }

  public mutating func incrementNumber() {
    self.number += 1
  }

  public func getNumber() -> Int {
    return self.number
  }

}
