// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import UIKit

@objcMembers public class PopupModel: NSObject, ObservableObject, AutocompleteResultConsumer {
  @Published var matches: [PopupMatch]
  weak var delegate: AutocompleteResultConsumerDelegate?

  public init(matches: [PopupMatch], delegate: AutocompleteResultConsumerDelegate?) {
    self.matches = matches
    self.delegate = delegate
  }

  // MARK: AutocompleteResultConsumer

  public func updateMatches(_ matches: [AutocompleteSuggestion], withAnimation: Bool) {
    self.matches = matches.map { match in PopupMatch(suggestion: match, pedal: nil) }
  }

  public func setTextAlignment(_ alignment: NSTextAlignment) {}
  public func setSemanticContentAttribute(_ semanticContentAttribute: UISemanticContentAttribute) {}
}
