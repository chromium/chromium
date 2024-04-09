// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

struct Provider: TimelineProvider {
  typealias Entry = SimpleEntry
  func placeholder(in context: Context) -> SimpleEntry {
    SimpleEntry(date: Date())
  }

  func getSnapshot(
    in context: Context,
    completion: @escaping (SimpleEntry) -> Void
  ) {
    let entry = SimpleEntry(date: Date())
    completion(entry)
  }

  func getTimeline(
    in context: Context,
    completion: @escaping (Timeline<Entry>) -> Void
  ) {
    let entry = SimpleEntry(date: Date())
    let timeline = Timeline(entries: [entry], policy: .never)
    completion(timeline)
  }
}

struct SimpleEntry: TimelineEntry {
  let date: Date
}

@main
struct ChromeWidgets: WidgetBundle {
  init() {
    CrashHelper.configure()
  }
  @WidgetBundleBuilder
  var body: some Widget {
    QuickActionsWidget()
    SearchWidget()
    #if IOS_ENABLE_SHORTCUTS_WIDGET
      ShortcutsWidget()
    #endif
    SearchPasswordsWidget()
    DinoGameWidget()
    #if IOS_ENABLE_LOCKSCREEN_WIDGET
      #if IOS_AVAILABLE_LOCKSCREEN_WIDGET
        LockscreenLauncherSearchWidget()
        LockscreenLauncherIncognitoWidget()
        LockscreenLauncherVoiceSearchWidget()
        LockscreenLauncherGameWidget()
      #endif
    #endif
  }
}
