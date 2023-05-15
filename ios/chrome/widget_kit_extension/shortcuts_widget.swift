// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

struct ShortcutsWidget: Widget {
  // Changing 'kind' or deleting this widget will cause all installed instances of this widget to
  // stop updating and show the placeholder state.
  let kind: String = "ShortcutsWidget"

  var body: some WidgetConfiguration {
    StaticConfiguration(
      kind: kind,
      provider: Provider()
    ) { entry in
      ShortcutsWidgetEntryView(entry: entry)
    }
    .configurationDisplayName(
      Text("Shortcuts Widget")
    )
    .description(Text("Shortcuts Widget Description"))
    .supportedFamilies([.systemMedium])
  }
}

struct ShortcutsWidgetEntryView: View {

  let entry: Provider.Entry

  var body: some View {
    EmptyView()
  }
}
