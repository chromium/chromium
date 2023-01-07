// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A view that displays `NSAttributedString`s created by omnibox's
/// `AutocompleteMatchFormatter`.
struct OmniboxText: View {
  let nsAttributedString: NSAttributedString
  let highlightColor: Color?

  init(_ nsAttributedString: NSAttributedString, highlightColor: Color? = nil) {
    self.nsAttributedString = nsAttributedString
    self.highlightColor = highlightColor
  }

  // Strips off any foreground colors from the string and replaces them with a provided one
  func replaceTextColor(nsAttributedString: NSAttributedString, color: Color) -> NSAttributedString
  {

    let mutableAttributedString = NSMutableAttributedString(attributedString: nsAttributedString)

    let wholeStringRange = NSRange(location: 0, length: nsAttributedString.length)
    mutableAttributedString.removeAttribute(
      NSAttributedString.Key.foregroundColor, range: wholeStringRange)
    mutableAttributedString.addAttribute(
      NSAttributedString.Key.foregroundColor, value: UIColor(color), range: wholeStringRange)

    return mutableAttributedString
  }

  var body: some View {

    var attributedString = nsAttributedString
    if let highlightColor = highlightColor {
      attributedString = replaceTextColor(
        nsAttributedString: nsAttributedString, color: highlightColor)
    }

    if #available(iOS 15, *) {
      return Text(AttributedString(attributedString))
    } else {
      var text = Text("")
      // Loop over all the ranges of the attributed string and create a new
      // Text element with the right modifiers for each.
      attributedString.enumerateAttributes(
        in: NSRange(location: 0, length: attributedString.length), options: []
      ) { attributes, range, _ in
        var newText = Text(attributedString.attributedSubstring(from: range).string)

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
