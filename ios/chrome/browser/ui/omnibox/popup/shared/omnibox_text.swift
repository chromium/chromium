// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A view that displays `NSAttributedString`s created by omnibox's
/// `AutocompleteMatchFormatter`.
struct OmniboxText: View {
  let nsAttributedString: NSAttributedString

  init(_ nsAttributedString: NSAttributedString) {
    self.nsAttributedString = nsAttributedString
  }

  var body: some View {
    if #available(iOS 15, *) {
      return Text(AttributedString(nsAttributedString))
    } else {
      var text = Text("")
      // Loop over all the ranges of the attributed string and create a new
      // Text element with the right modifiers for each.
      nsAttributedString.enumerateAttributes(
        in: NSRange(location: 0, length: nsAttributedString.length), options: []
      ) { attributes, range, _ in
        var newText = Text(nsAttributedString.attributedSubstring(from: range).string)

        // This only bothers reading the attributes that are used in
        // AutocompleteMatchFormatter.
        if let color = attributes[.foregroundColor] as? UIColor {
          newText = newText.foregroundColor(Color(color))
        }

        if let font = attributes[.font] as? UIFont {
          newText = newText.font(Font(font))
        }

        if let baseline = attributes[.baselineOffset] as? NSNumber {
          newText = newText.baselineOffset(CGFloat(baseline.floatValue))
        }

        text = text + newText
      }
      return text
    }
  }
}
