// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

struct ConfigureQuickActionsWidgetEntry: TimelineEntry {
  let date: Date
  let useLens: Bool
}

struct ConfigureQuickActionsWidgetEntryProvider: TimelineProvider {
  func placeholder(in context: Context) -> ConfigureQuickActionsWidgetEntry {
    ConfigureQuickActionsWidgetEntry(date: Date(), useLens: false)
  }

  func shouldUseLens() -> Bool {
    let sharedDefaults: UserDefaults = AppGroupHelper.groupUserDefaults()
    let useLens: Bool =
      sharedDefaults.bool(
        forKey: WidgetConstants.QuickActionsWidget.isGoogleDefaultSearchEngineKey)
      && sharedDefaults.bool(
        forKey: WidgetConstants.QuickActionsWidget.enableLensInWidgetKey)
    return useLens
  }

  func getSnapshot(
    in context: Context,
    completion: @escaping (ConfigureQuickActionsWidgetEntry) -> Void
  ) {
    let entry = ConfigureQuickActionsWidgetEntry(
      date: Date(),
      useLens: shouldUseLens()
    )
    completion(entry)
  }

  func getTimeline(
    in context: Context,
    completion: @escaping (Timeline<Entry>) -> Void
  ) {
    let entry = ConfigureQuickActionsWidgetEntry(
      date: Date(),
      useLens: shouldUseLens()
    )
    let entries: [ConfigureQuickActionsWidgetEntry] = [entry]
    let timeline: Timeline = Timeline(entries: entries, policy: .never)
    completion(timeline)
  }
}

struct QuickActionsWidget: Widget {
  // Changing |kind| or deleting this widget will cause all installed instances of this widget to
  // stop updating and show the placeholder state.
  let kind: String = "QuickActionsWidget"

  var body: some WidgetConfiguration {
    StaticConfiguration(
      kind: kind,
      provider: ConfigureQuickActionsWidgetEntryProvider()
    ) { entry in
      QuickActionsWidgetEntryView(entry: entry)
    }
    .configurationDisplayName(
      Text("IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_DISPLAY_NAME")
    )
    .description(Text("IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_DESCRIPTION"))
    .supportedFamilies([.systemMedium])
  }
}

struct QuickActionsWidgetEntryView: View {
  var entry: ConfigureQuickActionsWidgetEntry
  @Environment(\.redactionReasons) var redactionReasons
  private let searchAreaHeight: CGFloat = 92
  private let separatorHeight: CGFloat = 32
  private let incognitoA11yLabel: LocalizedStringKey =
    "IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_INCOGNITO_A11Y_LABEL"
  private let voiceSearchA11yLabel: LocalizedStringKey =
    "IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_VOICE_SEARCH_A11Y_LABEL"
  private let lensA11yLabel: LocalizedStringKey =
    "IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_LENS_A11Y_LABEL"
  private let qrA11yLabel: LocalizedStringKey =
    "IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_QR_SCAN_A11Y_LABEL"

  func symbolWithName(symbolName: String, system: Bool) -> some View {
    let image = system ? Image(systemName: symbolName) : Image(symbolName)
    return image.foregroundColor(Color("widget_actions_icon_color")).font(
      .system(size: 20, weight: .medium)
    ).imageScale(.medium)
  }

  var body: some View {
    VStack(spacing: 0) {
      ZStack {
        Color("widget_background_color")
          .unredacted()
        VStack {
          Spacer()
          Link(destination: WidgetConstants.QuickActionsWidget.searchUrl) {
            ZStack {
              RoundedRectangle(cornerRadius: 26)
                .frame(height: 52)
                .foregroundColor(Color("widget_search_bar_color"))
              HStack(spacing: 12) {
                Image("widget_chrome_logo")
                  .clipShape(Circle())
                  // Without .clipShape(Circle()), in the redacted/placeholder
                  // state the Widget shows an rectangle placeholder instead of
                  // a circular one.
                  .padding(.leading, 8)
                  .unredacted()
                Text("IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_TITLE")
                  .font(.subheadline)
                  .foregroundColor(Color("widget_text_color"))
                Spacer()
              }
            }
            .frame(minWidth: 0, maxWidth: .infinity)
            .padding([.leading, .trailing], 11)
          }
          .accessibility(
            label:
              Text(
                "IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_SEARCH_A11Y_LABEL"
              )
          )
          Spacer()
        }
        .frame(height: searchAreaHeight)
      }
      ZStack {
        Rectangle()
          .foregroundColor(Color("widget_actions_row_background_color"))
          .frame(minWidth: 0, maxWidth: .infinity)
        HStack {
          // Show interactive buttons if the widget is fully loaded, and show
          // the custom placeholder otherwise.
          if redactionReasons.isEmpty {
            Link(destination: WidgetConstants.QuickActionsWidget.incognitoUrl) {
              symbolWithName(symbolName: "widget_incognito_icon", system: false)
                .frame(minWidth: 0, maxWidth: .infinity)
            }
            .accessibility(label: Text(incognitoA11yLabel))
            Separator(height: separatorHeight)
            Link(
              destination: WidgetConstants.QuickActionsWidget.voiceSearchUrl
            ) {
              symbolWithName(symbolName: "mic", system: true)
                .frame(minWidth: 0, maxWidth: .infinity)
            }
            .accessibility(label: Text(voiceSearchA11yLabel))
            Separator(height: separatorHeight)
            if entry.useLens {
              Link(destination: WidgetConstants.QuickActionsWidget.lensUrl) {
                symbolWithName(symbolName: "widget_lens_icon", system: false)
                  .frame(minWidth: 0, maxWidth: .infinity)
              }
              .accessibility(label: Text(lensA11yLabel))
            } else {
              Link(destination: WidgetConstants.QuickActionsWidget.qrCodeUrl) {
                symbolWithName(symbolName: "qrcode", system: true)
                  .frame(minWidth: 0, maxWidth: .infinity)
              }
              .accessibility(label: Text(qrA11yLabel))
            }
          } else {
            ButtonPlaceholder()
            Separator(height: separatorHeight)
            ButtonPlaceholder()
            Separator(height: separatorHeight)
            ButtonPlaceholder()
          }
        }
        .frame(minWidth: 0, maxWidth: .infinity)
        .padding([.leading, .trailing], 11)
      }
    }
  }
}

struct Separator: View {
  let height: CGFloat
  var body: some View {
    RoundedRectangle(cornerRadius: 1)
      .foregroundColor(Color("widget_separator_color"))
      .frame(width: 2, height: height)
  }
}

struct ButtonPlaceholder: View {
  private let widthOfPlaceholder: CGFloat = 28
  var body: some View {
    Group {
      RoundedRectangle(cornerRadius: 4, style: .continuous)
        .frame(width: widthOfPlaceholder, height: widthOfPlaceholder)
        .foregroundColor(Color("widget_text_color"))
        .opacity(0.3)
    }.frame(minWidth: 0, maxWidth: .infinity)
  }
}
