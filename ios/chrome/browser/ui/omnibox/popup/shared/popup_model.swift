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

/// An extension of `@Published` that only publishes a new value when there
/// is an actual change.
/// TODO(crbug.com/1315110): See if this should be moved to a shared location.
@propertyWrapper class PublishedOnChange<Value: Equatable> {
  @Published private var value: Value

  public var projectedValue: Published<Value>.Publisher {
    get {
      return $value
    }
    set {
      $value = newValue
    }
  }
  var wrappedValue: Value {
    get {
      return value
    }
    set {
      guard newValue != value else {
        return
      }
      value = newValue
    }
  }
  init(wrappedValue value: Value) {
    self.value = value
  }

  public init(initialValue value: Value) {
    self.value = value
  }
}

@objcMembers public class PopupModel: NSObject, ObservableObject, AutocompleteResultConsumer {
  @Published var sections: [PopupMatchSection]

  @Published var highlightedMatchIndexPath: IndexPath?

  weak var delegate: AutocompleteResultConsumerDelegate?

  /// Holds how much leading space there is from the edge of the popup view to
  /// where the omnibox starts.
  @PublishedOnChange var omniboxLeadingSpace: CGFloat = 0

  /// Holds how much trailing space there is from where the omnibox ends to
  /// the edge of the popup view.
  @PublishedOnChange var omniboxTrailingSpace: CGFloat = 0

  public init(
    matches: [[PopupMatch]], headers: [String], delegate: AutocompleteResultConsumerDelegate?
  ) {
    assert(headers.count == matches.count)
    self.sections = zip(headers, matches).map { tuple in
      PopupMatchSection(header: tuple.0, matches: tuple.1)
    }
    self.delegate = delegate
  }

  // MARK: AutocompleteResultConsumer

  public func updateMatches(_ matchGroups: [AutocompleteSuggestionGroup], withAnimation: Bool) {
    // Reset highlight state.
    self.highlightedMatchIndexPath = nil

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
      indexPath.row = sections[indexPath.section].matches.count - 1
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
