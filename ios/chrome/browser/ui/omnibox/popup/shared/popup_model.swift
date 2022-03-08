// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import UIKit

@objcMembers
public class PopupModel: NSObject, ObservableObject, AutocompleteResultConsumer {
  @Published var matches: [PopupMatch]
  @Published var highlightedMatchIndex: Int?
  weak var delegate: AutocompleteResultConsumerDelegate?

  public init(matches: [PopupMatch], delegate: AutocompleteResultConsumerDelegate?) {
    self.matches = matches
    self.delegate = delegate
  }

  // MARK: AutocompleteResultConsumer

  public func updateMatches(_ matches: [AutocompleteSuggestion], withAnimation: Bool) {
    // Reset highlight state.
    self.highlightedMatchIndex = nil

    self.matches = matches.map { match in PopupMatch(suggestion: match, pedal: nil) }
  }

  public func setTextAlignment(_ alignment: NSTextAlignment) {}
  public func setSemanticContentAttribute(_ semanticContentAttribute: UISemanticContentAttribute) {}
}

// MARK: OmniboxSuggestionCommands

extension PopupModel: OmniboxSuggestionCommands {
  public func highlightNextSuggestion() {
    guard var index = self.highlightedMatchIndex else {
      // When nothing is highlighted, pressing Up Arrow doesn't do anything.
      return
    }

    if index == 0 {
      // Can't move up from first row. Call the delegate again so that the inline
      // autocomplete text is set again (in case the user exited the inline
      // autocomplete).
      self.delegate?.autocompleteResultConsumer(self, didHighlightRow: UInt(index))
      return
    }

    // There is a row above, move highlight there.
    index -= 1

    self.highlightedMatchIndex = index
    self.delegate?.autocompleteResultConsumer(self, didHighlightRow: UInt(index))
  }

  public func highlightPreviousSuggestion() {
    // Initialize the highlighted row to -1, so that pressing down when nothing
    // is highlighted highlights the first row (at index 0).
    var index = self.highlightedMatchIndex ?? -1

    if index < self.matches.count - 1 {
      // There is a row below, move highlight there.
      index += 1
    }

    // We call the delegate again even if we stayed on the last row so that the inline
    // autocomplete text is set again (in case the user exited the inline autocomplete).
    self.delegate?.autocompleteResultConsumer(self, didHighlightRow: UInt(index))
    self.highlightedMatchIndex = index
  }
}
