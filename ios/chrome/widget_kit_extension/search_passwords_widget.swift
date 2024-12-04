// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

struct SearchPasswordsWidget: Widget {
  // Changing `kind` or deleting this widget will cause all installed instances of this widget to
  // stop updating and show the placeholder state.
  let kind: String = "SearchPasswordsWidget"
  var body: some WidgetConfiguration {
    StaticConfiguration(kind: kind, provider: Provider()) { entry in
      SearchPasswordsWidgetEntryView(entry: entry)
    }
    .configurationDisplayName(
      Text("IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_PASSWORDS_DISPLAY_NAME")
    )
    .description(Text("IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_PASSWORDS_DESCRIPTION"))
    .supportedFamilies([.systemSmall])
    .crDisfavoredLocations()
    .crContentMarginsDisabled()
    .crContainerBackgroundRemovable(false)
  }
}

#if IOS_ENABLE_WIDGETS_FOR_MIM
  @available(iOS 17, *)
  struct SearchPasswordsWidgetConfigurable: Widget {
    // Changing `kind` or deleting this widget will cause all installed instances of this widget to
    // stop updating and show the placeholder state.
    let kind: String = "SearchPasswordsWidget"
    var body: some WidgetConfiguration {
      AppIntentConfiguration(
        kind: kind, intent: SelectProfileIntent.self, provider: ConfigurableProvider()
      ) { entry in
        SearchPasswordsWidgetEntryView(entry: entry)
      }
      .configurationDisplayName(
        Text("IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_PASSWORDS_DISPLAY_NAME")
      )
      .description(Text("IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_PASSWORDS_DESCRIPTION"))
      .supportedFamilies([.systemSmall])
      .crDisfavoredLocations()
      .crContentMarginsDisabled()
      .crContainerBackgroundRemovable(false)
    }
  }
#endif

struct SearchPasswordsWidgetEntryView: View {
  var entry: Provider.Entry

  var body: some View {
    SearchWidgetEntryViewTemplate(
      destinationURL: WidgetConstants.SearchPasswordsWidget.url,
      imageName: "widget_password_manager_logo",
      title: "IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_PASSWORDS_TITLE",
      accessibilityLabel: "IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_PASSWORDS_A11Y_LABEL", entry: entry)
  }
}
