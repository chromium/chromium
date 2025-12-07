// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

struct ConfigureQuickActionsWidgetEntry: TimelineEntry {
  let date: Date
  let useLens: Bool
  let useColorLensAndVoiceIcons: Bool
  let isPreview: Bool
  let avatar: Image?
  let gaiaID: String?
  let email: String?
  let deleted: Bool
}

struct ConfigureQuickActionsWidgetEntryProvider: TimelineProvider {
  func placeholder(in context: Context) -> ConfigureQuickActionsWidgetEntry {
    ConfigureQuickActionsWidgetEntry(
      date: Date(), useLens: false, useColorLensAndVoiceIcons: false, isPreview: true, avatar: nil,
      gaiaID: nil, email: nil, deleted: false)
  }

  func getSnapshot(
    in context: Context,
    completion: @escaping (ConfigureQuickActionsWidgetEntry) -> Void
  ) {
    let entry = ConfigureQuickActionsWidgetEntry(
      date: Date(),
      useLens: shouldUseLens(),
      useColorLensAndVoiceIcons: shouldUseColorLensAndVoiceIcons(),
      isPreview: context.isPreview,
      avatar: nil,
      gaiaID: nil,
      email: nil,
      deleted: false
    )
    completion(entry)
  }

  func getTimeline(
    in context: Context,
    completion: @escaping (Timeline<Entry>) -> Void
  ) {
    let entry = ConfigureQuickActionsWidgetEntry(
      date: Date(),
      useLens: shouldUseLens(),
      useColorLensAndVoiceIcons: shouldUseColorLensAndVoiceIcons(),
      isPreview: context.isPreview,
      avatar: nil,
      gaiaID: nil,
      email: nil,
      deleted: false
    )
    let entries: [ConfigureQuickActionsWidgetEntry] = [entry]
    let timeline: Timeline = Timeline(entries: entries, policy: .never)
    completion(timeline)
  }
}

struct QuickActionsWidget: Widget {
  // Changing 'kind' or deleting this widget will cause all installed instances of this widget to
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
    .crDisfavoredLocations()
    .crContentMarginsDisabled()
    .crContainerBackgroundRemovable(false)
  }
}

#if IOS_ENABLE_WIDGETS_FOR_MIM
  struct QuickActionsWidgetConfigurable: Widget {
    // Changing 'kind' or deleting this widget will cause all installed instances of this widget to
    // stop updating and show the placeholder state.
    let kind: String = "QuickActionsWidget"

    var body: some WidgetConfiguration {
      AppIntentConfiguration(
        kind: kind,
        intent: SelectAccountIntent.self,
        provider: ConfigurableQuickActionsWidgetEntryProvider()
      ) { entry in
        QuickActionsWidgetEntryView(entry: entry)
      }
      .configurationDisplayName(
        Text("IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_DISPLAY_NAME")
      )
      .description(Text("IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_DESCRIPTION"))
      .supportedFamilies([.systemMedium])
      .crDisfavoredLocations()
      .crContentMarginsDisabled()
      .crContainerBackgroundRemovable(false)
    }
  }

  // Advises WidgetKit when to update a widget’s display.
  struct ConfigurableQuickActionsWidgetEntryProvider: AppIntentTimelineProvider {

    func placeholder(in context: Context) -> ConfigureQuickActionsWidgetEntry {
      ConfigureQuickActionsWidgetEntry(
        date: Date(), useLens: false, useColorLensAndVoiceIcons: false, isPreview: true,
        avatar: nil, gaiaID: nil, email: nil, deleted: false)
    }

    func snapshot(for configuration: SelectAccountIntent, in context: Context) async
      -> ConfigureQuickActionsWidgetEntry
    {
      let avatar: Image? = configuration.avatar()
      let gaiaID: String? = configuration.gaia()
      let email: String? = configuration.email()
      let deleted: Bool = configuration.deleted()

      let entry = ConfigureQuickActionsWidgetEntry(
        date: Date(),
        useLens: shouldUseLens(),
        useColorLensAndVoiceIcons: shouldUseColorLensAndVoiceIcons(),
        isPreview: context.isPreview,
        avatar: avatar,
        gaiaID: gaiaID,
        email: email,
        deleted: deleted
      )
      return entry
    }

    func timeline(for configuration: SelectAccountIntent, in context: Context) async -> Timeline<
      ConfigureQuickActionsWidgetEntry
    > {
      let avatar: Image? = configuration.avatar()
      let gaiaID: String? = configuration.gaia()
      let email: String? = configuration.email()
      let deleted: Bool = configuration.deleted()

      let entry = ConfigureQuickActionsWidgetEntry(
        date: Date(),
        useLens: shouldUseLens(),
        useColorLensAndVoiceIcons: shouldUseColorLensAndVoiceIcons(),
        isPreview: context.isPreview,
        avatar: avatar,
        gaiaID: gaiaID,
        email: email,
        deleted: deleted
      )
      let entries: [ConfigureQuickActionsWidgetEntry] = [entry]
      let timeline: Timeline = Timeline(entries: entries, policy: .never)
      return timeline
    }
  }
#endif

func shouldUseLens() -> Bool {
  let sharedDefaults: UserDefaults = AppGroupHelper.groupUserDefaults()
  let useLens: Bool =
    sharedDefaults.bool(
      forKey: WidgetConstants.QuickActionsWidget.isGoogleDefaultSearchEngineKey)
    && sharedDefaults.bool(
      forKey: WidgetConstants.QuickActionsWidget.enableLensInWidgetKey)
  return useLens
}

func shouldUseColorLensAndVoiceIcons() -> Bool {
  // On iOS 15, color icons are not supported in widget, always return false
  // as no icon would be displayed.
  // On iOS 16, color icons are displayed in monochrome, so still present
  // the monochrome icon as it may be better adapted.
  if #available(iOS 17, *) {
    return shouldUseLens()
  }
  return false
}

struct QuickActionsWidgetEntryView: View {
  var entry: ConfigureQuickActionsWidgetEntry
  @Environment(\.colorScheme) var colorScheme: ColorScheme
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
    // The account to display was deleted (entry.deleted can only be true if
    // IOS_ENABLE_WIDGETS_FOR_MIM is true).
    if entry.deleted && !entry.isPreview {
      MediumWidgetDeletedAccountView()
    } else {
      VStack(spacing: 0) {
        ZStack {
          VStack {
            Spacer()
            ZStack {
              RoundedRectangle(cornerRadius: 26)
                .frame(height: 52)
                .foregroundColor(Color("widget_search_bar_color"))
                // This is needed so that the voice over will see the widget as a button and not as
                // an image.
                .accessibilityAddTraits(.isButton)
                .accessibilityLabel(
                  Text("IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_SEARCH_A11Y_LABEL")
                )
              HStack(spacing: 12) {
                Image("widget_chrome_logo")
                  .clipShape(Circle())
                  // Without .clipShape(Circle()), in the redacted/placeholder
                  // state the Widget shows an rectangle placeholder instead of
                  // a circular one.
                  .padding(.leading, 8)
                  .unredacted()
                  .accessibilityHidden(true)
                Text("IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_TITLE")
                  .font(.subheadline)
                  .foregroundColor(Color("widget_text_color"))
                  .accessibilityHidden(true)
                Spacer()
                #if IOS_ENABLE_WIDGETS_FOR_MIM
                  Avatar(entry: entry)
                #endif
              }
            }
            .frame(minWidth: 0, maxWidth: .infinity)
            .padding([.leading, .trailing], 11)
            .widgetURL(
              destinationURL(url: WidgetConstants.QuickActionsWidget.searchUrl, gaia: entry.gaiaID))
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
              Link(
                destination: destinationURL(
                  url: WidgetConstants.QuickActionsWidget.incognitoUrl, gaia: entry.gaiaID)
              ) {
                symbolWithName(symbolName: "widget_incognito_icon", system: false)
                  .frame(minWidth: 0, maxWidth: .infinity)
              }
              .accessibilityLabel(Text(incognitoA11yLabel))
              Separator(height: separatorHeight)
              Link(
                destination: destinationURL(
                  url: WidgetConstants.QuickActionsWidget.voiceSearchUrl, gaia: entry.gaiaID)
              ) {
                symbolWithName(symbolName: "widget_voice_icon", system: false)
                  .symbolRenderingMode(
                    (colorScheme == .light && entry.useColorLensAndVoiceIcons)
                      ? .multicolor : .monochrome
                  )
                  .frame(minWidth: 0, maxWidth: .infinity)
              }
              .accessibilityLabel(Text(voiceSearchA11yLabel))
              Separator(height: separatorHeight)
              if entry.useLens {
                Link(
                  destination: destinationURL(
                    url: WidgetConstants.QuickActionsWidget.lensUrl, gaia: entry.gaiaID)
                ) {
                  symbolWithName(symbolName: "widget_lens_icon", system: false)
                    .symbolRenderingMode(
                      (colorScheme == .light && entry.useColorLensAndVoiceIcons)
                        ? .multicolor : .monochrome
                    )
                    .frame(minWidth: 0, maxWidth: .infinity)
                }
                .accessibilityLabel(Text(lensA11yLabel))
              } else {
                Link(
                  destination: destinationURL(
                    url: WidgetConstants.QuickActionsWidget.qrCodeUrl, gaia: entry.gaiaID)
                ) {
                  symbolWithName(symbolName: "qrcode", system: true)
                    .frame(minWidth: 0, maxWidth: .infinity)
                }
                .accessibilityLabel(Text(qrA11yLabel))
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
      .crContainerBackground(Color("widget_background_color").unredacted())
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

struct Avatar: View {
  var entry: ConfigureQuickActionsWidgetEntry
  var body: some View {
    if entry.isPreview {
      Circle()
        .foregroundColor(Color("widget_text_color"))
        .opacity(0.2)
        .frame(width: 35, height: 35)
        .padding(.trailing, 8)
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
        .frame(width: 35, height: 35)
        .padding(.trailing, 8)
    }
  }
}
