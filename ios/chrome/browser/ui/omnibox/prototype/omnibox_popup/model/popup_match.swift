// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

@objcMembers class PopupMatch: NSObject, Identifiable {
  let title: String
  let subtitle: String?
  let url: URL?

  /// Some suggestions can be appended to omnibox text in order to refine the query or URL.
  let isAppendable: Bool

  /// Some suggestions are opened in another tab.
  let isTabMatch: Bool

  let pedal: Pedal?

  init(
    title: String, subtitle: String?, url: URL?, isAppendable: Bool, isTabMatch: Bool, pedal: Pedal?
  ) {
    self.title = title
    self.subtitle = subtitle
    self.url = url
    self.isAppendable = isAppendable
    self.isTabMatch = isTabMatch
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
    pedal: nil)
  static let long = PopupMatch(
    title: "1292459 - Overflow menu is displayed on top of NTP ...",
    subtitle: "bugs.chromium.org/p/chromium/issues/detail?id=1292459",
    url: URL(string: "https://bugs.chromium.org/p/chromium/issues/detail?id=1292459"),
    isAppendable: false,
    isTabMatch: false,
    pedal: nil)
  static let pedal = PopupMatch(
    title: "Set Default browser",
    subtitle: "Make Chrome your default browser",
    url: nil,
    isAppendable: false,
    isTabMatch: false,
    pedal: Pedal(title: "Click here"))
  static let appendable = PopupMatch(
    title: "is appendable",
    subtitle: nil,
    url: nil,
    isAppendable: true,
    isTabMatch: false,
    pedal: nil)
  static let tab_match = PopupMatch(
    title: "Home",
    subtitle: "https://chromium.org/chromium-projects/",
    url: nil,
    isAppendable: false,
    isTabMatch: true,
    pedal: nil)
  static let added = PopupMatch(
    title: "New Match",
    subtitle: "a new match",
    url: nil,
    isAppendable: false,
    isTabMatch: false,
    pedal: Pedal(title: "Click here"))
  static let previews = [short, long, pedal, appendable, tab_match]
}
