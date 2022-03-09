// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import UIKit

/// A match group consists of a title and a list of matches.
/// Use empty string to hide the section header.
typealias PopupMatchGroup = (header: String, matches: [PopupMatch])

@objcMembers public class PopupModel: NSObject, ObservableObject, AutocompleteResultConsumer {
  @Published var matches: [PopupMatchGroup]

  @Published var highlightedMatchIndexPath: IndexPath?

  weak var delegate: AutocompleteResultConsumerDelegate?

  public init(
    matches: [[PopupMatch]], headers: [String], delegate: AutocompleteResultConsumerDelegate?
  ) {
    assert(headers.count == matches.count)
    self.matches = zip(headers, matches).map { tuple in (header: tuple.0, matches: tuple.1) }
    self.delegate = delegate
  }

  // MARK: AutocompleteResultConsumer

  public func updateMatches(_ matchGroups: [AutocompleteSuggestionGroup], withAnimation: Bool) {
    // Reset highlight state.
    self.highlightedMatchIndexPath = nil

    self.matches = matchGroups.map { group in
      (
        header: group.title ?? String(),
        matches: group.suggestions.map { match in PopupMatch(suggestion: match, pedal: nil) }
      )
    }

  }

  public func setTextAlignment(_ alignment: NSTextAlignment) {}
  public func setSemanticContentAttribute(_ semanticContentAttribute: UISemanticContentAttribute) {}
}

// MARK: OmniboxSuggestionCommands

extension PopupModel: OmniboxSuggestionCommands {
  public func highlightNextSuggestion() {
    guard var indexPath = self.highlightedMatchIndexPath else {
      // When nothing is highlighted, pressing Up Arrow doesn't do anything.
      return
    }

    if indexPath.section == 0 && indexPath.row == 0 {
      // Can't move up from first row. Call the delegate again so that the inline
      // autocomplete text is set again (in case the user exited the inline
      // autocomplete).
      self.delegate?.autocompleteResultConsumer(
        self, didHighlightRow: UInt(indexPath.row), inSection: UInt(indexPath.section))
      return
    }

    // There is a row above, move highlight there.
    if indexPath.row == 0 && indexPath.section > 0 {
      // Move to the previous section
      indexPath.section -= 1
      indexPath.row = matches[indexPath.section].matches.count - 1
    } else {
      indexPath.row -= 1
    }

    self.highlightedMatchIndexPath = indexPath
    self.delegate?.autocompleteResultConsumer(
      self, didHighlightRow: UInt(indexPath.row), inSection: UInt(indexPath.section))
  }

  public func highlightPreviousSuggestion() {
    // Initialize the highlighted index path to section:0, row:-1, so that pressing down when nothing
    // is highlighted highlights the first row (at index 0).
    var indexPath = self.highlightedMatchIndexPath ?? IndexPath(row: -1, section: 0)

    // If there's a row below in current section, move highlight there
    if indexPath.row < matches[indexPath.section].matches.count - 1 {
      indexPath.row += 1
    } else if indexPath.section < matches.count - 1 {
      // Move to the next section
      indexPath.row = 0
      indexPath.section += 1
    }

    // We call the delegate again even if we stayed on the last row so that the inline
    // autocomplete text is set again (in case the user exited the inline autocomplete).
    self.delegate?.autocompleteResultConsumer(
      self, didHighlightRow: UInt(indexPath.row), inSection: UInt(indexPath.section))
    self.highlightedMatchIndexPath = indexPath
  }
}
