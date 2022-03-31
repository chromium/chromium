// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

@objcMembers public class PopupMatch: NSObject, Identifiable {
  // The underlying suggestion backing all the data.
  let suggestion: AutocompleteSuggestion

  var text: String {
    return suggestion.text?.string ?? ""
  }

  var detailText: String? {
    return suggestion.detailText?.string
  }

  /// Some suggestions can be appended to omnibox text in order to refine the
  /// query or URL.
  var isAppendable: Bool {
    return suggestion.isAppendable
  }

  /// Some suggestions are opened in another tab.
  var isTabMatch: Bool {
    return suggestion.isTabMatch
  }

  /// Some suggestions can be deleted with a swipe-to-delete gesture.
  var supportsDeletion: Bool {
    return suggestion.supportsDeletion
  }

  /// The pedal for this suggestion.
  var pedal: OmniboxPedal? {
    return suggestion.pedal
  }

  /// The image shown on the leading edge of the row (an icon, a favicon,
  /// etc.).
  lazy var image = suggestion.icon.map { icon in PopupImage(icon: icon) }

  public init(suggestion: AutocompleteSuggestion) {
    self.suggestion = suggestion
  }

  public var id: String {
    return text
  }
}

extension PopupMatch {
  class FakeAutocompleteSuggestion: NSObject, AutocompleteSuggestion {
    let text: NSAttributedString?
    let detailText: NSAttributedString?
    let isAppendable: Bool
    let isTabMatch: Bool
    let supportsDeletion: Bool
    let icon: OmniboxIcon?
    let pedal: (OmniboxIcon & OmniboxPedal)?

    let hasAnswer = false
    let isURL = false
    let numberOfLines = 1
    let isTailSuggestion = false
    let commonPrefix = ""

    init(
      text: String, detailText: String? = nil, isAppendable: Bool = false, isTabMatch: Bool = false,
      supportsDeletion: Bool = false, icon: OmniboxIcon? = nil, pedal: OmniboxPedalData? = nil
    ) {
      self.text = NSAttributedString(string: text, attributes: [:])
      self.detailText = detailText.flatMap { string in
        NSAttributedString(string: string, attributes: [:])
      }
      self.isAppendable = isAppendable
      self.isTabMatch = isTabMatch
      self.supportsDeletion = supportsDeletion
      self.icon = icon
      self.pedal = pedal
    }
  }

  static let short = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "Google",
      detailText: "google.com",
      icon: FakeOmniboxIcon.suggestionIcon))
  static let long = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "1292459 - Overflow menu is displayed on top of NTP ...",
      detailText: "bugs.chromium.org/p/chromium/issues/detail?id=1292459",
      icon: FakeOmniboxIcon.favicon))
  static let pedal = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "clear browsing data",
      pedal: OmniboxPedalData(
        title: "Click here", subtitle: "PAR → NYC",
        accessibilityHint: "a11y hint", imageName: "pedal_dino", action: {})))
  static let appendable = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "is appendable",
      isAppendable: true,
      icon: FakeOmniboxIcon.suggestionAnswerIcon))
  static let tabMatch = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "Home",
      isTabMatch: true))
  static let added = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "New Match",
      pedal: OmniboxPedalData(
        title: "Click here", subtitle: "NYC → PAR",
        accessibilityHint: "a11y hint", imageName: "pedal_dino", action: {})))
  static let supportsDeletion = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "supports deletion",
      isAppendable: true,
      supportsDeletion: true))
  static let previews = [short, long, pedal, appendable, tabMatch, supportsDeletion]
}
