// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import WidgetKit

// Entry containing info needed for multi-profile.
struct ConfigureWidgetEntry: TimelineEntry {
  let date: Date
  let isPreview: Bool
  let avatar: Image?
  let gaiaID: String?
  let deleted: Bool
}

struct Provider: TimelineProvider {
  typealias Entry = ConfigureWidgetEntry
  func placeholder(in context: Context) -> Entry {
    Entry(date: Date(), isPreview: true, avatar: nil, gaiaID: nil, deleted: false)
  }

  func getSnapshot(
    in context: Context,
    completion: @escaping (Entry) -> Void
  ) {
    let entry = Entry(
      date: Date(), isPreview: context.isPreview, avatar: nil, gaiaID: nil, deleted: false)
    completion(entry)
  }

  func getTimeline(
    in context: Context,
    completion: @escaping (Timeline<Entry>) -> Void
  ) {
    let entry = Entry(
      date: Date(), isPreview: context.isPreview, avatar: nil, gaiaID: nil, deleted: false)
    let timeline = Timeline(entries: [entry], policy: .never)
    completion(timeline)
  }
}

@available(iOS 17, *)
struct ConfigurableProvider: AppIntentTimelineProvider {
  typealias Entry = ConfigureWidgetEntry

  func placeholder(in: Self.Context) -> Entry {
    Entry(date: Date(), isPreview: true, avatar: nil, gaiaID: nil, deleted: false)
  }
  func snapshot(for configuration: SelectAccountIntent, in context: Context) async -> Entry {
    let avatar: Image? = configuration.avatar()
    let gaiaID: String? = configuration.gaia()
    let deleted: Bool = configuration.deleted()

    return Entry(
      date: Date(), isPreview: context.isPreview, avatar: avatar, gaiaID: gaiaID,
      deleted: deleted
    )
  }
  func timeline(for configuration: SelectAccountIntent, in context: Context) async -> Timeline<
    Entry
  > {
    let avatar: Image? = configuration.avatar()
    let gaiaID: String? = configuration.gaia()
    let deleted: Bool = configuration.deleted()

    return Timeline(
      entries: [
        Entry(
          date: Date(), isPreview: context.isPreview, avatar: avatar, gaiaID: gaiaID,
          deleted: deleted)
      ], policy: .never
    )
  }

}
