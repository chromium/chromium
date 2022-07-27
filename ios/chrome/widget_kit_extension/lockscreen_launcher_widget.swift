// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

#if IOS_ENABLE_LOCKSCREEN_EXTENSION
  #if IOS_AVAILABLE_LOCKSCREEN_EXTENSION

    enum LockscreenLauncherWidgetType {
      case search, incognito, voiceSearch, dinoGame

      struct Configuration {
        let displayName: String, description: String, imageName: String, widgetURL: URL

        var supportedFamilies: [WidgetFamily] {
          if #available(iOS 16, *) {
            return [.accessoryCircular]
          }
          return []
        }
      }

      // TODO(crbug.com/1347565): Add translations.
      var configuration: Configuration {
        switch self {
        case .search:
          return Configuration(
            displayName: "Search",
            description: "Search",
            imageName: "lockscreen_chrome_logo",
            widgetURL: WidgetConstants.LockscreenLauncherWidget.searchUrl)
        case .incognito:
          return Configuration(
            displayName: "Incognito Search",
            description: "Incognito Search",
            imageName: "lockscreen_incognito_logo",
            widgetURL: WidgetConstants.LockscreenLauncherWidget.incognitoUrl)
        case .voiceSearch:
          return Configuration(
            displayName: "Voice Search",
            description: "Voice Search",
            imageName: "lockscreen_voicesearch_logo",
            widgetURL: WidgetConstants.LockscreenLauncherWidget.voiceSearchUrl)
        case .dinoGame:
          return Configuration(
            displayName: "Dino Game",
            description: "Dino Game",
            imageName: "lockscreen_game_logo",
            widgetURL: WidgetConstants.LockscreenLauncherWidget.gameUrl)
        }
      }
    }

    struct LockscreenLauncherWidgetEntryView: View {
      let entry: Provider.Entry
      let configuration: LockscreenLauncherWidgetType.Configuration

      var body: some View {
        let configuration = self.configuration
        // TODO(crbug.com/1347565): Add a11y label.
        Image(configuration.imageName)
          .widgetURL(configuration.widgetURL)
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

  #endif  // IOS_AVAILABLE_LOCKSCREEN_EXTENSION
#endif  // IOS_ENABLE_LOCKSCREEN_EXTENSION
