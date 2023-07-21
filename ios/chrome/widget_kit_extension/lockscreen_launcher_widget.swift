// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

#if IOS_ENABLE_LOCKSCREEN_WIDGET
  #if IOS_AVAILABLE_LOCKSCREEN_WIDGET

    enum LockscreenLauncherWidgetType {
      case search, incognito, voiceSearch, dinoGame

      struct Configuration {
        let displayName: LocalizedStringKey, description: LocalizedStringKey, imageName: String,
          accessibilityLabel: LocalizedStringKey,
          widgetURL: URL

        var supportedFamilies: [WidgetFamily] {
          if #available(iOS 16, *) {
            return [.accessoryCircular]
          }
          return []
        }
      }

      var configuration: Configuration {
        switch self {
        case .search:
          return Configuration(
            displayName: "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_SEARCH_DISPLAY_NAME",
            description: "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_SEARCH_DESCRIPTION",
            imageName: "lockscreen_chrome_logo",
            accessibilityLabel:
              "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_SEARCH_A11Y_LABEL",
            widgetURL: WidgetConstants.LockscreenLauncherWidget.searchUrl)
        case .incognito:
          return Configuration(
            displayName: "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_INCOGNITO_DISPLAY_NAME",
            description: "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_INCOGNITO_DESCRIPTION",
            imageName: "lockscreen_incognito_logo",
            accessibilityLabel:
              "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_INCOGNITO_A11Y_LABEL",
            widgetURL: WidgetConstants.LockscreenLauncherWidget.incognitoUrl)
        case .voiceSearch:
          return Configuration(
            displayName:
              "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_VOICESEARCH_DISPLAY_NAME",
            description: "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_VOICESEARCH_DESCRIPTION",
            imageName: "lockscreen_voicesearch_logo",
            accessibilityLabel:
              "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_VOICESEARCH_A11Y_LABEL",
            widgetURL: WidgetConstants.LockscreenLauncherWidget.voiceSearchUrl)
        case .dinoGame:
          return Configuration(
            displayName: "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_GAME_DISPLAY_NAME",
            description: "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_GAME_DESCRIPTION",
            imageName: "lockscreen_game_logo",
            accessibilityLabel: "IDS_IOS_WIDGET_KIT_EXTENSION_LOCKSCREEN_LAUNCHER_GAME_A11Y_LABEL",
            widgetURL: WidgetConstants.LockscreenLauncherWidget.gameUrl)
        }
      }
    }

    func lockScreenWidgetBackground() -> some View {
      if #available(iOS 16.0, *) {
        return AccessoryWidgetBackground()
      } else {
        // Widget only supports iOS16+
        return EmptyView()
      }
    }

    struct LockscreenLauncherWidgetEntryView: View {
      let entry: Provider.Entry
      let configuration: LockscreenLauncherWidgetType.Configuration

      var body: some View {
        let configuration = self.configuration
        ZStack {
          Image(configuration.imageName)
            .renderingMode(.template)
            .foregroundColor(.white)
        }
        .widgetURL(configuration.widgetURL)
        .accessibilityElement()
        .accessibilityLabel(configuration.accessibilityLabel)
        .crContainerBackground(lockScreenWidgetBackground())
      }
    }

    func lockscreenLauncherWidgetConfiguration(
      ofKind kind: String, forType type: LockscreenLauncherWidgetType
    ) -> some WidgetConfiguration {
      let configuration = type.configuration
      return StaticConfiguration(kind: kind, provider: Provider()) { entry in
        LockscreenLauncherWidgetEntryView(entry: entry, configuration: configuration)
      }
      .configurationDisplayName(
        Text(configuration.displayName)
      )
      .description(Text(configuration.description))
      .supportedFamilies(configuration.supportedFamilies)
      .crDisfavoredLocations()
      .crContainerBackgroundRemovable(false)
    }

    struct LockscreenLauncherSearchWidget: Widget {
      // Changing |kind| or deleting this widget will cause all installed instances of this widget
      // to stop updating and show the placeholder state.
      let kind: String = "LockscreenLauncherSearchWidget"

      var body: some WidgetConfiguration {
        lockscreenLauncherWidgetConfiguration(ofKind: kind, forType: .search)
      }
    }

    struct LockscreenLauncherIncognitoWidget: Widget {
      // Changing |kind| or deleting this widget will cause all installed instances of this widget
      // to stop updating and show the placeholder state.
      let kind: String = "LockscreenLauncherIncognitoWidget"

      var body: some WidgetConfiguration {
        lockscreenLauncherWidgetConfiguration(ofKind: kind, forType: .incognito)
      }
    }

    struct LockscreenLauncherVoiceSearchWidget: Widget {
      // Changing |kind| or deleting this widget will cause all installed instances of this widget
      // to stop updating and show the placeholder state.
      let kind: String = "LockscreenLauncherVoiceSearchWidget"

      var body: some WidgetConfiguration {
        lockscreenLauncherWidgetConfiguration(ofKind: kind, forType: .voiceSearch)
      }
    }

    struct LockscreenLauncherGameWidget: Widget {
      // Changing |kind| or deleting this widget will cause all installed instances of this widget
      // to stop updating and show the placeholder state.
      let kind: String = "LockscreenLauncherGameWidget"

      var body: some WidgetConfiguration {
        lockscreenLauncherWidgetConfiguration(ofKind: kind, forType: .dinoGame)
      }
    }

  #endif  // IOS_AVAILABLE_LOCKSCREEN_WIDGET
#endif  // IOS_ENABLE_LOCKSCREEN_WIDGET
