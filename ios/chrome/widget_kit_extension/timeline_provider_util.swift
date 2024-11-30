// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

#if IOS_ENABLE_WIDGETS_FOR_MIM
  @available(iOS 17, *)
  struct ConfigurableProvider: AppIntentTimelineProvider {
    func placeholder(in: Self.Context) -> SimpleEntry {
      SimpleEntry(date: Date())
    }
    func snapshot(for configuration: SelectProfileIntent, in context: Context) async -> SimpleEntry
    {
      return SimpleEntry(date: Date())
    }
    func timeline(for configuration: SelectProfileIntent, in context: Context) async -> Timeline<
      SimpleEntry
    > {
      return Timeline(entries: [SimpleEntry(date: Date())], policy: .never)
    }

  }
#endif

struct SimpleEntry: TimelineEntry {
  let date: Date
}
