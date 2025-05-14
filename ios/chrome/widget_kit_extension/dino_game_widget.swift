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

@available(iOS 17, *)
struct DinoGameWidgetConfigurable: Widget {
  // Changing `kind` or deleting this widget will cause all installed instances of this widget to
  // stop updating and show the placeholder state.
  let kind: String = "DinoGameWidget"
  var body: some WidgetConfiguration {
    AppIntentConfiguration(
      kind: kind, intent: SelectAccountIntent.self, provider: ConfigurableProvider()
    ) { entry in
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
  var entry: ConfigureWidgetEntry
  @Environment(\.redactionReasons) var redactionReasons
  var body: some View {
    // The account to display was deleted (entry.deleted can only be true if
    // WidgetForMIMAvailable is enabled).
    if entry.deleted && !entry.isPreview {
      SmallWidgetDeletedAccountView()
    } else {
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
              if ChromeWidgetsMain.WidgetForMIMAvailable {
                AvatarForDinoGame(entry: entry)
              }
            }
            .padding([.leading, .bottom], 16)
          }
        }
      }
      .widgetURL(destinationURL(url: WidgetConstants.DinoGameWidget.url, gaia: entry.gaiaID))
      .accessibility(
        label: Text("IDS_IOS_WIDGET_KIT_EXTENSION_GAME_A11Y_LABEL")
      )
      // Background is not used as the image takes the whole widget.
      .crContainerBackground(Color("widget_background_color").unredacted())
    }
  }
}

struct AvatarForDinoGame: View {
  var entry: ConfigureWidgetEntry
  var body: some View {
    if entry.isPreview {
      Circle()
        .foregroundColor(Color("widget_text_color"))
        .opacity(0.2)
        .frame(width: 25, height: 25)
        .padding(.trailing, 16)
    } else if let avatar = entry.avatar {
      avatar
        .resizable()
        .clipShape(Circle())
        .unredacted()
        .scaledToFill()
        .frame(width: 25, height: 25)
        .padding(.trailing, 16)
    }
  }
}
