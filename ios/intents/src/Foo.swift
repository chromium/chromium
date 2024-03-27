// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import AppIntents

public struct FooIntent: AppIntent {
  public init() {}

  public static var title = LocalizedStringResource("Foo")
  public static var description = IntentDescription("Perform Foo.")

  public func perform() async throws -> some IntentResult {
    return .result()
  }
}
