// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

// Specifies the date of the current widget and indicates the widget's content.
struct ConfigureShortcutsWidgetEntry: TimelineEntry {
  // Date and time to update the widget’s content.
  let date: Date
  // A dictionary containing the most visited URLs
  // and their NTPTiles from the UserDefaults.
  let mostVisitedSites: [NSURL: NTPTile]
  // A Boolean value that indicates when the widget appears in the widget gallery.
  var isPreview: Bool = false
}

// Advises WidgetKit when to update a widget’s display.
struct ConfigureShortcutsWidgetEntryProvider: TimelineProvider {

  // A type that specifies the entry of the configured timeline entry of the widget.
  typealias Entry = ConfigureShortcutsWidgetEntry

  // Provides a timeline entry representing a placeholder version of the widget.
  func placeholder(in context: TimelineProviderContext) -> Entry {
    return Entry(date: Date(), mostVisitedSites: [NSURL: NTPTile]())
  }

  // This function is to load the most visited websites
  // from NTPTiles from the UserDefaults.
  func loadMostVisitedSites() -> [NSURL: NTPTile] {
    let sharedDefaults: UserDefaults = AppGroupHelper.groupUserDefaults()
    guard let data = sharedDefaults.object(forKey: "SuggestedItems") as? Data,
      let unarchiver = try? NSKeyedUnarchiver(forReadingFrom: data)
    else {
      return [:]
    }

    unarchiver.requiresSecureCoding = false

    guard
      let mostVisitedSites = unarchiver.decodeObject(forKey: NSKeyedArchiveRootObjectKey)
        as? [NSURL: NTPTile]
    else {
      return [:]
    }
    return mostVisitedSites
  }

  // Return an empty list if the user check from the widget gallery and not the home page.
  func initializeMostVisitedSites(isPreview: Bool) -> Entry {
    var entry = Entry(
      date: Date(),
      mostVisitedSites: (isPreview ? [:] : loadMostVisitedSites())
    )
    entry.isPreview = isPreview
    return entry
  }

  // Provides a timeline entry that represents the current time and state of a widget.
  func getSnapshot(
    in context: TimelineProviderContext,
    completion: @escaping (Entry) -> Void
  ) {
    let entry = initializeMostVisitedSites(isPreview: context.isPreview)
    completion(entry)
  }

  // Provides an array of timeline entries for the current time.
  func getTimeline(
    in context: TimelineProviderContext,
    completion: @escaping (Timeline<Entry>) -> Void
  ) {
    let entry = initializeMostVisitedSites(isPreview: context.isPreview)
    let entries = [entry]
    let timeline = Timeline(entries: entries, policy: .never)
    completion(timeline)
  }
}

// Provides the configuration and content of a widget to display on the Home screen.
struct ShortcutsWidget: Widget {
  // Changing 'kind' or deleting this widget will cause all installed instances of this widget to
  // stop updating and show the placeholder state.
  let kind: String = "ShortcutsWidget"
  let deviceModel = UIDevice.current.model
  var body: some WidgetConfiguration {
    StaticConfiguration(
      kind: kind,
      provider: ConfigureShortcutsWidgetEntryProvider()
    ) { entry in
      ShortcutsWidgetEntryView(entry: entry)
    }
    .configurationDisplayName(
      Text("IDS_IOS_WIDGET_KIT_EXTENSION_SHORTCUTS_DISPLAY_NAME")
    )
    .description(
      deviceModel == "iPhone"
        ? Text("IDS_IOS_WIDGET_KIT_EXTENSION_SHORTCUTS_DESCRIPTION_IPHONE")
        : Text("IDS_IOS_WIDGET_KIT_EXTENSION_SHORTCUTS_DESCRIPTION_IPAD")
    )
    .supportedFamilies([.systemMedium])
    .crDisfavoredLocations()
    .crContentMarginsDisabled()
  }
}

// Presents the shortcuts widget with SwiftUI views.
struct ShortcutsWidgetEntryView: View {

  let entry: ConfigureShortcutsWidgetEntry

  enum Dimensions {
    static let searchAreaHeight: CGFloat = 92
    static let separatorHeight: CGFloat = 32
    static let stackFramePadding: CGFloat = 11
    static let iconsPadding: CGFloat = 19
    static let placeholdersPadding: CGFloat = 1.0
  }

  enum Strings {
    static let widgetDisplayName = String(
      localized: "IDS_IOS_WIDGET_KIT_EXTENSION_SHORTCUTS_DISPLAY_NAME")
    static let searchA11yLabel = String(
      localized:
        "IDS_IOS_WIDGET_KIT_EXTENSION_SHORTCUTS_SEARCH_A11Y_LABEL")
    static let openShorcutLabelTemplate = String(
      localized: "IDS_IOS_WIDGET_KIT_EXTENSION_SHORTCUTS_OPEN_SHORTCUT_LABEL")
    static let noShortcutsAvailableTitle = String(
      localized: "IDS_IOS_WIDGET_KIT_EXTENSION_SHORTCUTS_NO_SHORTCUTS_AVAILABLE_LABEL")
  }

  enum Colors {
    static let widgetBackgroundColor = Color("widget_background_color")
    static let widgetMostVisitedSitesRow = Color("widget_actions_row_background_color")
    static let widgetTextColor = Color("widget_text_color")
    static let widgetSearchBarColor = Color("widget_search_bar_color")
  }

  // Create a chromewidgetkit:// url to open the given URL.
  private func convertURL(url: URL) -> URL {
    let query = URLQueryItem(name: "url", value: url.absoluteString)
    var urlcomps = URLComponents(
      url: WidgetConstants.ShortcutsWidget.open, resolvingAgainstBaseURL: false)!
    urlcomps.queryItems = [query]
    return urlcomps.url!
  }

  // Shows the search bar of the shortcuts widget.
  @ViewBuilder
  private var searchBar: some View {

    let cornerRadius: CGFloat = 26
    let height: CGFloat = 52
    let spacing: CGFloat = 12
    let padding: CGFloat = 8

    Link(destination: WidgetConstants.ShortcutsWidget.searchUrl) {
      ZStack {
        RoundedRectangle(cornerRadius: cornerRadius)
          .frame(height: height)
          .foregroundColor(Colors.widgetSearchBarColor)
        HStack(spacing: spacing) {
          Image("widget_chrome_logo")
            .clipShape(Circle())
            .padding(.leading, padding)
            .unredacted()
          Text(Strings.searchA11yLabel)
            .font(.subheadline)
            .foregroundColor(Colors.widgetTextColor)
          Spacer()
        }
      }
      .frame(minWidth: 0, maxWidth: .infinity)
      .padding([.leading, .trailing], Dimensions.stackFramePadding)
    }
    .accessibilityLabel(Strings.searchA11yLabel)
  }

  // Shows the widget with 4 shortcuts placeholder in the gallery view to respect user's privacy.
  public var websitesPlaceholder: some View {
    HStack(spacing: 3) {
      WebSitePlaceholder()
      SeparatorVertical()
      WebSitePlaceholder()
      SeparatorVertical()
      WebSitePlaceholder()
      SeparatorVertical()
      WebSitePlaceholder()
    }
    .frame(minWidth: 0, maxWidth: .infinity)
  }

  // Shows the "No shortcuts available" text when the user's delete
  // all his most visited websites from Chrome App.
  private var zeroVisitedSitesView: some View {
    WebsiteLabel(websiteTitle: Strings.noShortcutsAvailableTitle).padding(.leading, 10)
      .accessibilityLabel(Strings.noShortcutsAvailableTitle)
  }

  // Shows the shortcut's icon with website's title on the left.
  @ViewBuilder
  private func oneVisitedSitesView(ntpTile: NTPTile) -> some View {
    Link(destination: convertURL(url: ntpTile.url)) {
      HStack {
        WebsiteLogo(ntpTile: ntpTile).padding(.leading, 12)
        WebsiteLabel(
          websiteTitle: Strings.openShorcutLabelTemplate.replacingOccurrences(
            of: "WEBSITE_PLACEHOLDER", with: ntpTile.title ?? "")
        )
        .padding(.leading, 8)
      }
    }
    .accessibilityLabel(ntpTile.title)
  }

  // Shows the shortcuts containing the most visited websites.
  @ViewBuilder
  private func multipleVisitedSitesView(ntpTiles: [NTPTile]) -> some View {
    // The maximum number of sites that can be displayed.
    let maxNumberOfShortcuts = 4
    let numberOfShortcuts = min(ntpTiles.count, maxNumberOfShortcuts)

    ForEach(0..<numberOfShortcuts, id: \.self) { index in
      HStack(spacing: 0.5) {

        Link(destination: convertURL(url: ntpTiles[index].url)) {
          WebsiteLogo(ntpTile: ntpTiles[index])
        }
        .accessibilityLabel(ntpTiles[index].title)
      }
      .frame(minWidth: 0, maxWidth: .infinity)
      .padding([.leading, .trailing], Dimensions.iconsPadding)
      if index < numberOfShortcuts - 1 {
        SeparatorVertical()
      }
    }
  }

  var body: some View {
    VStack(spacing: 0) {
      searchBar.frame(height: Dimensions.searchAreaHeight)
      ZStack {
        Rectangle()
          .foregroundColor(Colors.widgetMostVisitedSitesRow)
          .frame(minWidth: 0, maxWidth: .infinity)
          .accessibilityLabel(Strings.widgetDisplayName)
        HStack {
          let ntpTiles = Array(entry.mostVisitedSites.values).sorted()

          if entry.isPreview {
            websitesPlaceholder
          } else {
            switch ntpTiles.count {
            case 0:
              zeroVisitedSitesView
            case 1:
              oneVisitedSitesView(ntpTile: ntpTiles[0])
            default:
              multipleVisitedSitesView(ntpTiles: ntpTiles)
            }
          }
          Spacer()
        }
        .frame(minWidth: 0, maxWidth: .infinity)
      }
      .frame(maxHeight: .infinity)
    }
    .crContainerBackground(
      Colors.widgetBackgroundColor.unredacted()
    )

  }
}

// Adds the comparable conformance to NTPTile
// to sort the NTPTiles
extension NTPTile: Comparable {
  public static func < (lhs: NTPTile, rhs: NTPTile) -> Bool {
    return lhs.position < rhs.position
  }
}

// Vertical `|` separator view between two shortcuts in a row of the Shortcuts widget.
struct SeparatorVertical: View {
  enum Dimensions {
    static let height: CGFloat = 32
    static let width: CGFloat = 2
    static let cornerRadius: CGFloat = 1
  }
  enum Colors {
    static let widgetSeparatorColor = Color("widget_separator_color")
  }
  var body: some View {
    RoundedRectangle(cornerRadius: Dimensions.cornerRadius)
      .foregroundColor(Colors.widgetSeparatorColor)
      .frame(width: Dimensions.width, height: Dimensions.height)
  }
}

// Generates the placeholder of the shortcut.
struct WebSitePlaceholder: View {
  enum Dimensions {
    static let placeholderSize: CGFloat = 28
  }
  enum Colors {
    static let placeholderBackgroundColor = Color("widget_text_color")
  }
  var body: some View {
    Circle()
      .frame(width: Dimensions.placeholderSize, height: Dimensions.placeholderSize)
      .foregroundColor(Colors.placeholderBackgroundColor)
      .opacity(0.2)
      .frame(minWidth: 0, maxWidth: .infinity)
  }
}

// Generates the logo of the shortcut.
struct WebsiteLogo: View {
  enum Dimensions {
    static let placeholderSize: CGFloat = 28
    static let cornerRadius: CGFloat = 4
    static let fontSize: CGFloat = 15
  }

  enum Colors {
    static let shortcutBackgroundColor = Color("widget_background_color")
    static let shortcutTextColor = Color("widget_text_color")
  }

  let ntpTile: NTPTile

  var backgroundColor: Color {
    if let backgroundColor = ntpTile.fallbackBackgroundColor {
      return Color(backgroundColor)
    } else {
      return Colors.shortcutBackgroundColor
    }
  }
  var fallbackMonogram: String {
    return ntpTile.fallbackMonogram ?? ""
  }
  var fallbackTextColor: Color {
    if let fallbackTextColor = ntpTile.fallbackTextColor {
      return Color(fallbackTextColor)
    } else {
      return Colors.shortcutTextColor
    }
  }
  var faviconImage: Image? {
    let faviconFilePath =
      AppGroupHelper.widgetsFaviconsFolder().appendingPathComponent(
        ntpTile.faviconFileName)

    guard let uiImage = UIImage(contentsOfFile: faviconFilePath.path) else {
      return nil
    }
    return Image(uiImage: uiImage)
  }

  @ViewBuilder
  private func backgroundWithLogo(faviconImage: Image) -> some View {
    ZStack {
      faviconImage.resizable()
        .frame(width: Dimensions.placeholderSize, height: Dimensions.placeholderSize)
        .cornerRadius(Dimensions.cornerRadius)
    }
  }

  var monogramIcon: some View {
    ZStack {
      RoundedRectangle(cornerRadius: Dimensions.cornerRadius, style: .continuous)
        .frame(width: Dimensions.placeholderSize, height: Dimensions.placeholderSize)
        .foregroundColor(backgroundColor)
      monogramText
    }
  }

  var monogramText: some View {
    Text(verbatim: fallbackMonogram)
      .font(.system(size: Dimensions.fontSize))
      .foregroundColor(fallbackTextColor)
  }

  var body: some View {
    if let faviconImage = faviconImage {
      backgroundWithLogo(faviconImage: faviconImage).cornerRadius(Dimensions.cornerRadius)
    } else {
      monogramIcon.cornerRadius(Dimensions.cornerRadius)
    }
  }
}

// Generates the title of the shortcut.
struct WebsiteLabel: View {
  let websiteTitle: String
  let fontSize: CGFloat = 13
  var body: some View {
    Text(verbatim: self.websiteTitle)
      .font(.system(size: fontSize))
      .opacity(0.59)
  }
}
