// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

struct DinoGameWidgetConfigurable: Widget {
  // Changing `kind` or deleting this widget will cause all installed instances of this widget to
  // stop updating and show the placeholder state.
  let kind: String = "DinoGameWidget"
  var body: some WidgetConfiguration {
    AppIntentConfiguration(
      kind: kind, intent: SelectAccountIntent.self, provider: ConfigurableProvider()
    ) { entry in
      DinoGameWidgetEntryView(
        destinationURL: destinationURL(
          url: WidgetConstants.DinoGameWidget.url, gaia: entry.gaiaID), entry: entry)
    }
    .configurationDisplayName(
      Text("IDS_IOS_WIDGET_KIT_EXTENSION_GAME_DISPLAY_NAME")
    )
    .description(Text("IDS_IOS_WIDGET_KIT_EXTENSION_GAME_DESCRIPTION"))
    .supportedFamilies([.systemSmall])
    .crDisfavoredLocations()
    .contentMarginsDisabled()
    .containerBackgroundRemovable(false)
  }
}

struct DinoGameWidgetEntryView: View {
  let destinationURL: URL
  let background = "widget_dino_background"
  let backgroundPlaceholder = "widget_dino_background_placeholder"
  var entry: ConfigureWidgetEntry
  @Environment(\.redactionReasons) var redactionReasons
  var body: some View {
    // The account to display was deleted.
    if entry.deleted && !entry.isPreview {
      SmallWidgetDeletedAccountView()
    } else {
      ZStack {
        Image(redactionReasons.isEmpty ? background : backgroundPlaceholder)
          .resizable()
          .unredacted()
          // This is needed so that the voice over will see the widget as a button and not as
          // an image.
          .accessibilityAddTraits(.isButton)
          .accessibilityRemoveTraits(.isImage)
          .accessibilityLabel(Text("IDS_IOS_WIDGET_KIT_EXTENSION_GAME_A11Y_LABEL"))
        VStack(alignment: .leading, spacing: 0) {
          Spacer()
            .frame(minWidth: 0, maxWidth: .infinity)
          HStack {
            Text("IDS_IOS_WIDGET_KIT_EXTENSION_GAME_TITLE")
              .foregroundColor(Color("widget_text_color"))
              .fontWeight(.semibold)
              .font(.subheadline)
              .accessibilityHidden(true)
            Spacer()
            AvatarForDinoGame(entry: entry)
          }
          .padding([.leading, .bottom], 16)
        }
      }
      .widgetURL(destinationURL)
      // Background is not used as the image takes the whole widget.
      .containerBackground(for: .widget) {
        Color("widget_background_color").unredacted()
      }
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
    } else if let avatar = entry.avatar,
      let email = entry.email
    {
      avatar
        .resizable()
        .clipShape(Circle())
        .accessibilityLabel(
          String(localized: "IDS_IOS_WIDGET_KIT_EXTENSION_AVATAR_A11Y_LABEL") + email
        )
        .unredacted()
        .scaledToFill()
        .frame(width: 25, height: 25)
        .padding(.trailing, 16)
    }
  }
}
