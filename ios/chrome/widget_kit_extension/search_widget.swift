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

#if IOS_ENABLE_WIDGETS_FOR_MIM
  struct SearchWidgetConfigurable: Widget {
    // Changing 'kind' or deleting this widget will cause all installed instances of this widget to
    // stop updating and show the placeholder state.
    let kind: String = "SearchWidget"
    var body: some WidgetConfiguration {
      AppIntentConfiguration(
        kind: kind, intent: SelectAccountIntent.self, provider: ConfigurableProvider()
      ) { entry in
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
#endif

struct SearchWidgetEntryView: View {
  var entry: ConfigureWidgetEntry

  var body: some View {
    // The account to display was deleted (entry.deleted can only be true if
    // IOS_ENABLE_WIDGETS_FOR_MIM is true).
    if entry.deleted && !entry.isPreview {
      SmallWidgetDeletedAccountView()
    } else {
      SearchWidgetEntryViewTemplate(
        destinationURL: destinationURL(url: WidgetConstants.SearchWidget.url, gaia: entry.gaiaID),
        imageName: "widget_chrome_logo",
        title: entry.avatar != nil
          ? "IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_AVATAR_TITLE"
          : "IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_TITLE",
        accessibilityLabel: "IDS_IOS_WIDGET_KIT_EXTENSION_SEARCH_A11Y_LABEL", entry: entry)
    }
  }
}

struct SearchWidgetEntryViewTemplate: View {
  let destinationURL: URL
  let imageName: String
  let title: LocalizedStringKey
  let accessibilityLabel: LocalizedStringKey
  var entry: ConfigureWidgetEntry

  var body: some View {
    ZStack {
      VStack(alignment: .leading, spacing: 0) {
        ZStack {
          RoundedRectangle(cornerRadius: 26)
            .frame(height: 52)
            .foregroundColor(Color("widget_search_bar_color"))
            // This is needed so that the voice over will see the widget as a button and not as
            // an image.
            .accessibilityAddTraits(.isButton)
            .accessibilityLabel(Text(accessibilityLabel))
          HStack(spacing: 0) {
            Image(imageName)
              .clipShape(Circle())
              .padding(.leading, 8)
              .unredacted()
              .accessibilityHidden(true)
            Spacer()
          }
        }
        .frame(minWidth: 0, maxWidth: .infinity)
        .padding([.leading, .trailing], 11)
        .padding(.top, 16)
        Spacer()
        HStack {
          Text(title)
            .foregroundColor(Color("widget_text_color"))
            .fontWeight(.semibold)
            .font(.subheadline)
            .padding([.leading, .bottom], 16)
            .accessibilityHidden(true)
          Spacer()
          #if IOS_ENABLE_WIDGETS_FOR_MIM
            AvatarForSearch(entry: entry)
          #endif
        }
      }
    }
    .widgetURL(destinationURL)
    .crContainerBackground(
      Color("widget_background_color")
        .unredacted())
  }
}

#if IOS_ENABLE_WIDGETS_FOR_MIM
  struct AvatarForSearch: View {
    var entry: ConfigureWidgetEntry
    var body: some View {
      if entry.isPreview {
        Circle()
          .foregroundColor(Color("widget_text_color"))
          .opacity(0.2)
          .frame(width: 25, height: 25)
          .padding([.bottom, .trailing], 16)
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
          .padding([.bottom, .trailing], 16)
      }
    }
  }
#endif
