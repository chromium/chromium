// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

// Specifies the date of the current widget and indicates the widget's content.
struct ConfigureShortcutsWidgetEntry: TimelineEntry {
  // Date and time to update the widget’s content.
  let date: Date
  // A dictionary containing the most visited URLs
  // and their NTPTiles from the UserDefaults.
  let mostVisitedSites: [NSURL: NTPTile]
  // A Boolean value that indicates when the widget appears in the widget gallery.
  var isPreview: Bool = false
}

// Advises WidgetKit when to update a widget’s display.
struct ConfigureShortcutsWidgetEntryProvider: TimelineProvider {

  // A type that specifies the entry of the configured timeline entry of the widget.
  typealias Entry = ConfigureShortcutsWidgetEntry

  // Provides a timeline entry representing a placeholder version of the widget.
  func placeholder(in context: TimelineProviderContext) -> Entry {
    return Entry(date: Date(), mostVisitedSites: [NSURL: NTPTile]())
  }

  // This function is to load the most visited websites
  // from NTPTiles from the UserDefaults.
  func loadMostVisitedSites() -> [NSURL: NTPTile] {
    let sharedDefaults: UserDefaults = AppGroupHelper.groupUserDefaults()
    guard let data = sharedDefaults.object(forKey: "SuggestedItems") as? Data,
      let unarchiver = try? NSKeyedUnarchiver(forReadingFrom: data)
    else {
      return [:]
    }

    unarchiver.requiresSecureCoding = false

    guard
      let mostVisitedSites = unarchiver.decodeObject(forKey: NSKeyedArchiveRootObjectKey)
        as? [NSURL: NTPTile]
    else {
      return [:]
    }
    return mostVisitedSites
  }

  // Return an empty list if the user check from the widget gallery and not the home page.
  func initializeMostVisitedSites(isPreview: Bool) -> Entry {
    var entry = Entry(
      date: Date(),
      mostVisitedSites: (isPreview ? [:] : loadMostVisitedSites())
    )
    entry.isPreview = isPreview
    return entry
  }

  // Provides a timeline entry that represents the current time and state of a widget.
  func getSnapshot(
    in context: TimelineProviderContext,
    completion: @escaping (Entry) -> Void
  ) {
    let entry = initializeMostVisitedSites(isPreview: context.isPreview)
    completion(entry)
  }

  // Provides an array of timeline entries for the current time.
  func getTimeline(
    in context: TimelineProviderContext,
    completion: @escaping (Timeline<Entry>) -> Void
  ) {
    let entry = initializeMostVisitedSites(isPreview: context.isPreview)
    let entries = [entry]
    let timeline = Timeline(entries: entries, policy: .never)
    completion(timeline)
  }
}

// Provides the configuration and content of a widget to display on the Home screen.
struct ShortcutsWidget: Widget {
  // Changing 'kind' or deleting this widget will cause all installed instances of this widget to
  // stop updating and show the placeholder state.
  let kind: String = "ShortcutsWidget"
  let deviceModel = UIDevice.current.model
  var body: some WidgetConfiguration {
    StaticConfiguration(
      kind: kind,
      provider: ConfigureShortcutsWidgetEntryProvider()
    ) { entry in
      ShortcutsWidgetEntryView(entry: entry)
    }
    .configurationDisplayName(
      Text("IDS_IOS_WIDGET_KIT_EXTENSION_SHORTCUTS_DISPLAY_NAME")
    )
    .description(
      deviceModel == "iPhone"
        ? Text("IDS_IOS_WIDGET_KIT_EXTENSION_SHORTCUTS_DESCRIPTION_IPHONE")
        : Text("IDS_IOS_WIDGET_KIT_EXTENSION_SHORTCUTS_DESCRIPTION_IPAD")
    )
    .supportedFamilies([.systemMedium])
  }
}

// Presents the shortcuts widget with SwiftUI views.
struct ShortcutsWidgetEntryView: View {

  let entry: ConfigureShortcutsWidgetEntry

  var body: some View {
    EmptyView()
  }
}
