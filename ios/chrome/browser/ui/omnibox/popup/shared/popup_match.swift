// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

@objcMembers public class PopupMatch: NSObject, Identifiable {
  let title: String
  let subtitle: String?
  let url: URL?

  /// Some suggestions can be appended to omnibox text in order to refine the query or URL.
  let isAppendable: Bool

  /// Some suggestions are opened in another tab.
  let isTabMatch: Bool

  /// Some suggestions can be deleted with a swipe-to-delete gesture.
  let supportsDeletion: Bool

  let pedal: Pedal?

  public init(match: AutocompleteSuggestion) {
    self.title = match.text().string
    self.subtitle = match.detailText()?.string
    self.isAppendable = match.isAppendable()
    self.isTabMatch = match.isTabMatch()
    self.supportsDeletion = match.supportsDeletion()
    self.pedal = nil
    self.url = nil  // TODO: remove url
  }

  public init(
    title: String, subtitle: String? = nil, url: URL?, isAppendable: Bool, isTabMatch: Bool,
    supportsDeletion: Bool, pedal: Pedal?
  ) {
    self.title = title
    self.subtitle = subtitle
    self.url = url
    self.isAppendable = isAppendable
    self.isTabMatch = isTabMatch
    self.supportsDeletion = supportsDeletion
    self.pedal = pedal
  }

  public var id: String {
    return title
  }
}

extension PopupMatch {
  static let short = PopupMatch(
    title: "Google",
    subtitle: "google.com",
    url: URL(string: "http://www.google.com"),
    isAppendable: false,
    isTabMatch: false,
    supportsDeletion: false,
    pedal: nil)
  static let long = PopupMatch(
    title: "1292459 - Overflow menu is displayed on top of NTP ...",
    subtitle: "bugs.chromium.org/p/chromium/issues/detail?id=1292459",
    url: URL(string: "https://bugs.chromium.org/p/chromium/issues/detail?id=1292459"),
    isAppendable: false,
    isTabMatch: false,
    supportsDeletion: false,
    pedal: nil)
  static let pedal = PopupMatch(
    title: "clear browsing data",
    url: nil,
    isAppendable: false,
    isTabMatch: false,
    supportsDeletion: false,
    pedal: Pedal(title: "Clear Browsing Data"))
  static let appendable = PopupMatch(
    title: "is appendable",
    subtitle: nil,
    url: nil,
    isAppendable: true,
    isTabMatch: false,
    supportsDeletion: false,
    pedal: nil)
  static let tab_match = PopupMatch(
    title: "Home",
    subtitle: "https://chromium.org/chromium-projects/",
    url: nil,
    isAppendable: false,
    isTabMatch: true,
    supportsDeletion: false,
    pedal: nil)
  static let added = PopupMatch(
    title: "New Match",
    subtitle: "a new match",
    url: nil,
    isAppendable: false,
    isTabMatch: false,
    supportsDeletion: false,
    pedal: Pedal(title: "Click here"))
  static let supports_deletion = PopupMatch(
    title: "supports deletion",
    subtitle: nil,
    url: nil,
    isAppendable: true,
    isTabMatch: false,
    supportsDeletion: true,
    pedal: nil)
  static let previews = [short, long, pedal, appendable, tab_match, supports_deletion]
}
