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
}

struct Provider: TimelineProvider {
  typealias Entry = ConfigureWidgetEntry
  func placeholder(in context: Context) -> Entry {
    Entry(date: Date(), isPreview: true, avatar: nil)
  }

  func getSnapshot(
    in context: Context,
    completion: @escaping (Entry) -> Void
  ) {
    let entry = Entry(date: Date(), isPreview: context.isPreview, avatar: nil)
    completion(entry)
  }

  func getTimeline(
    in context: Context,
    completion: @escaping (Timeline<Entry>) -> Void
  ) {
    let entry = Entry(date: Date(), isPreview: context.isPreview, avatar: nil)
    let timeline = Timeline(entries: [entry], policy: .never)
    completion(timeline)
  }
}

#if IOS_ENABLE_WIDGETS_FOR_MIM
  @available(iOS 17, *)
  struct ConfigurableProvider: AppIntentTimelineProvider {
    typealias Entry = ConfigureWidgetEntry

    func placeholder(in: Self.Context) -> Entry {
      Entry(date: Date(), isPreview: true, avatar: nil)
    }
    func snapshot(for configuration: SelectProfileIntent, in context: Context) async -> Entry {
      let avatar: Image? = configuration.avatarForProfile(profile: configuration.profile)
      return Entry(date: Date(), isPreview: context.isPreview, avatar: avatar)
    }
    func timeline(for configuration: SelectProfileIntent, in context: Context) async -> Timeline<
      Entry
    > {
      let avatar: Image? = configuration.avatarForProfile(profile: configuration.profile)
      return Timeline(
        entries: [Entry(date: Date(), isPreview: context.isPreview, avatar: avatar)], policy: .never
      )
    }

  }
#endif
