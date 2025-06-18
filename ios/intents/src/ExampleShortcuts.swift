// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import AppIntents

public struct ExampleShortcuts: AppShortcutsProvider {
  public init() {}

  public static var appShortcuts: [AppShortcut] {
    AppShortcut(
      intent: ExampleIntent(),
      phrases: [
        "Foo",
        "Foo \(.applicationName)",
      ],
      shortTitle: "Foo Shortcut",
      systemImageName: ""
    )
  }
}
