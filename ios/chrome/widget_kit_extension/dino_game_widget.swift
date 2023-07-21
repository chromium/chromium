// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

struct DinoGameWidget: Widget {
  // Changing |kind| or deleting this widget will cause all installed instances of this widget to
  // stop updating and show the placeholder state.
  let kind: String = "DinoGameWidget"
  var body: some WidgetConfiguration {
    StaticConfiguration(kind: kind, provider: Provider()) { entry in
      DinoGameWidgetEntryView(entry: entry)
    }
    .configurationDisplayName(
      Text("IDS_IOS_WIDGET_KIT_EXTENSION_GAME_DISPLAY_NAME")
    )
    .description(Text("IDS_IOS_WIDGET_KIT_EXTENSION_GAME_DESCRIPTION"))
    .supportedFamilies([.systemSmall])
    .crDisfavoredLocations()
    .crContentMarginsDisabled()
    .crContainerBackgroundRemovable(false)
  }
}

struct DinoGameWidgetEntryView: View {
  let background = "widget_dino_background"
  let backgroundPlaceholder = "widget_dino_background_placeholder"
  var entry: Provider.Entry
  @Environment(\.redactionReasons) var redactionReasons
  var body: some View {
    // We wrap this widget in a link on top of using `widgetUrl` so that the voice over will treat
    // the widget as one tap target. Without the wrapping, voice over treats the content within
    // the widget as multiple tap targets.
    Link(destination: WidgetConstants.DinoGameWidget.url) {
      ZStack {
        Image(redactionReasons.isEmpty ? background : backgroundPlaceholder)
          .resizable()
          .unredacted()
        VStack(alignment: .leading, spacing: 0) {
          Spacer()
            .frame(minWidth: 0, maxWidth: .infinity)
          HStack {
            Text("IDS_IOS_WIDGET_KIT_EXTENSION_GAME_TITLE")
              .foregroundColor(Color("widget_text_color"))
              .fontWeight(.semibold)
              .font(.subheadline)
              .lineLimit(1)
            Spacer()
          }
          .padding([.leading, .bottom], 16)
        }
      }
    }
    .widgetURL(WidgetConstants.DinoGameWidget.url)
    .accessibility(
      label: Text("IDS_IOS_WIDGET_KIT_EXTENSION_GAME_A11Y_LABEL")
    )
    // Background is not used as the image takes the whole widget.
    .crContainerBackground(Color("widget_background_color").unredacted())
  }
}
