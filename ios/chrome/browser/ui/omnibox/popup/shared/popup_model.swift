// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import UIKit

/// A match group consists of a title and a list of matches.
/// Use empty string to hide the section header.
@objcMembers public class PopupMatchSection: NSObject {
  let header: String
  let matches: [PopupMatch]
  public init(header: String, matches: [PopupMatch]) {
    self.header = header
    self.matches = matches
  }
}

@objcMembers public class PopupModel: NSObject, ObservableObject, AutocompleteResultConsumer {
  @Published var sections: [PopupMatchSection]

  @Published var highlightedMatchIndexPath: IndexPath?

  /// Index of the preselected section when no row is highlighted.
  var preselectedSectionIndex: Int

  weak var delegate: AutocompleteResultConsumerDelegate?

  public init(
    matches: [[PopupMatch]], headers: [String], delegate: AutocompleteResultConsumerDelegate?
  ) {
    assert(headers.count == matches.count)
    self.sections = zip(headers, matches).map { tuple in
      PopupMatchSection(header: tuple.0, matches: tuple.1)
    }
    self.delegate = delegate
    preselectedSectionIndex = 0
  }

  // MARK: AutocompleteResultConsumer

  public func updateMatches(
    _ matchGroups: [AutocompleteSuggestionGroup], preselectedMatchGroupIndex: NSInteger
  ) {
    // Reset highlight state.
    self.highlightedMatchIndexPath = nil
    self.preselectedSectionIndex = preselectedMatchGroupIndex

    self.sections = matchGroups.map { group in
      PopupMatchSection(
        header: group.title ?? String(),
        matches: group.suggestions.map { match in PopupMatch(suggestion: match) }
      )
    }

  }

  public func setTextAlignment(_ alignment: NSTextAlignment) {}
  public func setSemanticContentAttribute(_ semanticContentAttribute: UISemanticContentAttribute) {}
}

// MARK: OmniboxSuggestionCommands

extension PopupModel: OmniboxSuggestionCommands {
  public func highlightNextSuggestion() {
    // Pressing Up Arrow when there are no suggestions does nothing.
    if sections.isEmpty || sections.first!.matches.isEmpty {
      return
    }
    var indexPath = IndexPath()
    if self.highlightedMatchIndexPath != nil {
      indexPath = self.highlightedMatchIndexPath!
    } else if self.preselectedSectionIndex > 0 && self.preselectedSectionIndex - 1 < sections.count
    {
      /// When no row is highlighted, highlight the first row of the preselected section.
      /// If there is a row above the preselected row, select this row.
      indexPath = IndexPath(
        row: sections[preselectedSectionIndex - 1].matches.count,
        section: self.preselectedSectionIndex - 1)
    } else {
      /// If there are no rows above the preselected row, do nothing.
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
      indexPath.row = sections[indexPath.section].matches.count - 1
    } else {
      indexPath.row -= 1
    }

    self.highlightedMatchIndexPath = indexPath
    self.delegate?.autocompleteResultConsumer(
      self, didHighlightRow: UInt(indexPath.row), inSection: UInt(indexPath.section))
  }

  public func highlightPreviousSuggestion() {
    // Pressing Down Arrow when there are no suggestions does nothing.
    if sections.isEmpty || sections.first!.matches.isEmpty {
      return
    }
    assert(self.preselectedSectionIndex < sections.count)
    /// Initialize the highlighted index path to (`preselectedSectionIndex`, -1)
    /// so that pressing down when nothing is highlighted highlights the first row of the preselected section.
    var indexPath =
      self.highlightedMatchIndexPath ?? IndexPath(row: -1, section: self.preselectedSectionIndex)

    // If there's a row below in current section, move highlight there
    if indexPath.row < sections[indexPath.section].matches.count - 1 {
      indexPath.row += 1
    } else if indexPath.section < sections.count - 1 {
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
