// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

struct SearchWidget: Widget {
  // Changing |kind| or deleting this widget will cause all installed instances of this widget to
  // stop updating and show the placeholder state.
  let kind: String = "SearchWidget"
  var body: some WidgetConfiguration {
    StaticConfiguration(kind: kind, provider: Provider()) { entry in
      SearchWidgetEntryView(entry: entry)
    }
    .configurationDisplayName(
      Text("IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_DISPLAY_NAME")
    )
    .description(Text("IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_DESCRIPTION"))
    .supportedFamilies([.systemSmall])
    .crDisfavoredLocations()
    .crContentMarginsDisabled()
    .crContainerBackgroundRemovable(false)
  }
}

struct SearchWidgetEntryView: View {
  var entry: Provider.Entry

  var body: some View {
    SearchWidgetEntryViewTemplate(
      destinationURL: WidgetConstants.SearchWidget.url, imageName: "widget_chrome_logo",
      title: "IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_TITLE",
      accessibilityLabel: "IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_A11Y_LABEL", entry: entry)
  }
}

struct SearchWidgetEntryViewTemplate: View {
  let destinationURL: URL
  let imageName: String
  let title: LocalizedStringKey
  let accessibilityLabel: LocalizedStringKey
  var entry: Provider.Entry

  var body: some View {
    // We wrap this widget in a link on top of using `widgetUrl` so that the voice over will treat
    // the widget as one tap target. Without the wrapping, voice over treats the content within
    // the widget as multiple tap targets.
    Link(destination: destinationURL) {
      ZStack {
        VStack(alignment: .leading, spacing: 0) {
          ZStack {
            RoundedRectangle(cornerRadius: 26)
              .frame(height: 52)
              .foregroundColor(Color("widget_search_bar_color"))
            HStack(spacing: 0) {
              Image(imageName)
                .clipShape(Circle())
                .padding(.leading, 8)
                .unredacted()
              Spacer()
            }
          }
          .frame(minWidth: 0, maxWidth: .infinity)
          .padding([.leading, .trailing], 11)
          .padding(.top, 16)
          Spacer()
          Text(title)
            .foregroundColor(Color("widget_text_color"))
            .fontWeight(.semibold)
            .font(.subheadline)
            .padding([.leading, .bottom, .trailing], 16)
        }
      }
    }
    .widgetURL(destinationURL)
    .accessibility(
      label: Text(accessibilityLabel)
    )
    .crContainerBackground(
      Color("widget_background_color")
        .unredacted())
  }
}
