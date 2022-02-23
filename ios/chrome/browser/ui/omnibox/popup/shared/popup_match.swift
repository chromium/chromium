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

  /// Closure to execute when the trailing button is tapped.
  let trailingButtonHandler: () -> Void

  /// Closure to execute when the match is deleted.
  let deletionHandler: () -> Void

  public init(
    title: String, subtitle: String?, url: URL?, isAppendable: Bool, isTabMatch: Bool,
    supportsDeletion: Bool, pedal: Pedal?, trailingButtonHandler: @escaping () -> Void = {},
    deletionHandler: @escaping () -> Void = {}
  ) {
    self.title = title
    self.subtitle = subtitle
    self.url = url
    self.isAppendable = isAppendable
    self.isTabMatch = isTabMatch
    self.supportsDeletion = supportsDeletion
    self.pedal = pedal
    self.trailingButtonHandler = trailingButtonHandler
    self.deletionHandler = deletionHandler
  }

  public var id: String {
    return title
  }

  func trailingButtonTapped() {
    trailingButtonHandler()
  }

  func selectedForDeletion() {
    deletionHandler()
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
    title: "Set Default browser",
    subtitle: "Make Chrome your default browser",
    url: nil,
    isAppendable: false,
    isTabMatch: false,
    supportsDeletion: false,
    pedal: Pedal(title: "Click here"))
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
