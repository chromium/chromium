// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI

@objcMembers public class PopupMatch: NSObject, Identifiable {
  // The underlying suggestion backing all the data.
  let suggestion: AutocompleteSuggestion

  var text: NSAttributedString {
    return suggestion.text ?? NSAttributedString(string: "")
  }

  var detailText: NSAttributedString? {
    return suggestion.detailText
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

  /// Some suggestions are from the clipboard provider.
  var isClipboardMatch: Bool {
    return suggestion.isClipboardMatch
  }

  /// Some suggestions can be deleted with a swipe-to-delete gesture.
  var supportsDeletion: Bool {
    return suggestion.supportsDeletion
  }

  /// Some suggestions are answers that are displayed inline, such as for weather or calculator.
  var hasAnswer: Bool {
    return suggestion.hasAnswer
  }

  /// Suggested number of lines to format |detailText|.
  var numberOfLines: Int {
    return suggestion.numberOfLines
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
    return text.string
  }
}

extension PopupMatch {
  class FakeAutocompleteSuggestion: NSObject, AutocompleteSuggestion {
    let text: NSAttributedString?
    let detailText: NSAttributedString?
    let isAppendable: Bool
    let isTabMatch: Bool
    let isClipboardMatch: Bool
    let supportsDeletion: Bool
    let icon: OmniboxIcon?
    let pedal: (OmniboxIcon & OmniboxPedal)?
    let suggestionGroupId: NSNumber? = 0
    let suggestionSectionId: NSNumber? = 0

    let hasAnswer: Bool
    let isURL = false
    let numberOfLines: Int
    let isTailSuggestion = false
    let commonPrefix = ""
    let matchTypeIcon: UIImage? = nil
    let isMatchTypeSearch = false
    let omniboxPreviewText: NSAttributedString? = nil
    let destinationUrl: CrURL? = nil

    init(
      text: String, detailText: String? = nil, isAppendable: Bool = false, isTabMatch: Bool = false,
      isClipboardMatch: Bool = false, supportsDeletion: Bool = false, icon: OmniboxIcon? = nil,
      hasAnswer: Bool = false, numberOfLines: Int = 1, pedal: OmniboxPedalData? = nil
    ) {
      self.text = NSAttributedString(string: text, attributes: [:])
      self.detailText = detailText.flatMap { string in
        NSAttributedString(string: string, attributes: [:])
      }
      self.isAppendable = isAppendable
      self.isTabMatch = isTabMatch
      self.isClipboardMatch = isClipboardMatch
      self.supportsDeletion = supportsDeletion
      self.icon = icon
      self.pedal = pedal
      self.hasAnswer = hasAnswer
      self.numberOfLines = numberOfLines
    }

    init(
      attributedText: NSAttributedString, attributedDetailText: NSAttributedString? = nil,
      isAppendable: Bool = false, isTabMatch: Bool = false, isClipboardMatch: Bool = false,
      hasAnswer: Bool = false,
      supportsDeletion: Bool = false, icon: OmniboxIcon? = nil, numberOfLines: Int = 1,
      pedal: OmniboxPedalData? = nil
    ) {
      self.text = attributedText
      self.detailText = attributedDetailText
      self.isAppendable = isAppendable
      self.isTabMatch = isTabMatch
      self.isClipboardMatch = isClipboardMatch
      self.supportsDeletion = supportsDeletion
      self.icon = icon
      self.pedal = pedal
      self.hasAnswer = hasAnswer
      self.numberOfLines = numberOfLines
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
  static let arabic = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "ععععععععع ععععععععع ععععععععع عععععععع عععععععع عععععععع عععععععع عععععع عععععععل",
      detailText: "letter ع many times, and a single ل in the end",
      pedal: OmniboxPedalData(
        title: "Click here", subtitle: "PAR → NYC",
        accessibilityHint: "a11y hint", imageName: "pedal_dino", type: 123, incognito: false,
        action: { print("dino pedal clicked") })))
  static let pedal = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "This has pedal attached",
      detailText: "no pedal button in current design",
      pedal: OmniboxPedalData(
        title: "Click here", subtitle: "PAR → NYC",
        accessibilityHint: "a11y hint", imageName: "pedal_dino", type: 123,
        incognito: false,
        action: { print("dino pedal clicked") })))
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
        accessibilityHint: "a11y hint", imageName: "pedal_dino", type: 123, incognito: false,
        action: {})))
  static let supportsDeletion = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "supports deletion",
      isAppendable: true,
      supportsDeletion: true))
  static let definition = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      text: "Answer: definition",
      detailText:
        "this is a definition suggestion that has a very long text that should span multiple lines and not have a gradient fade out",
      isAppendable: true,
      supportsDeletion: false,
      hasAnswer: true,
      numberOfLines: 3
    ))
  // The blue attribued string is used to verify that keyboard highlighting overrides the attributes.
  static let blueAttributedText = PopupMatch(
    suggestion: FakeAutocompleteSuggestion(
      attributedText: NSAttributedString(
        string: "blue attr string",
        attributes: [
          NSAttributedString.Key.foregroundColor: UIColor.blue
        ]),
      isAppendable: true,
      supportsDeletion: true))

  static let previews = [
    short, definition, long, arabic, pedal, appendable, tabMatch, supportsDeletion,
    blueAttributedText,
  ]

}
