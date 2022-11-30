// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// PreferenceKey to listen to changes of a view's size.
struct SizePreferenceKey: PreferenceKey {
  static var defaultValue = CGSize.zero
  // This function determines how to combine the preference values for two
  // child views. In the absence of any better combination method, just use the
  // second value.
  static func reduce(value: inout CGSize, nextValue: () -> CGSize) {
    value = nextValue()
  }
}

/// A text view that clips long strings with a gradient mask.
struct GradientTextView: View {
  enum Dimensions {
    /// When the text size variations are less than this value, they are ignored
    /// so as to avoid triggering to many SwiftUI rendering cycles.
    static let epsilon: CGFloat = 1.0
  }

  // Text to be displayed.
  let text: NSAttributedString

  // Overrides the default/attributed string's built in text color.
  let highlightColor: Color?

  init(_ text: NSAttributedString, highlightColor: Color? = nil) {
    self.text = text
    self.highlightColor = highlightColor
  }

  /// Indicates if self.text is left-to-right or not.
  var isTextLTR: Bool {
    // Attempt to detect the language of the text to look up the writing direction.
    // This is not going to work well for mixed LTR/RTL text, but this is mainly
    // used for search suggestions and URLs, both of which are currently single-direction.
    let text = self.text.string
    guard
      let languageCode = CFStringTokenizerCopyBestStringLanguage(
        text as CFString, CFRange(location: 0, length: text.count))
    else {
      return true
    }

    return NSLocale.characterDirection(forLanguage: languageCode as String) == .leftToRight
  }

  /// Content text size as measured at rendering. Used to resize the container to not exceed the height of the text,
  /// as well as detecting when the text needs to be truncated.
  @State var textSize: CGSize = .zero

  var gradient: LinearGradient {

    let stops = [
      Gradient.Stop(color: .clear, location: 0.02),
      Gradient.Stop(color: .white, location: 0.12),
    ]

    if isTextLTR {
      return LinearGradient(
        gradient: Gradient(stops: stops), startPoint: .trailing, endPoint: .leading)
    } else {
      return LinearGradient(
        gradient: Gradient(stops: stops), startPoint: .leading, endPoint: .trailing)
    }

  }

  @Environment(\.layoutDirection) var layoutDirection: LayoutDirection

  var body: some View {
    // This is equivalent to left/right since the locale is ignored below.
    let alignment: Alignment = layoutDirection == .leftToRight ? .leading : .trailing

    ZStack {
      // Render text at full size inside a fixed-size container.
      // The container forces the text to always fit into the view's bounds.
      // It also allows to align the text contents to the leading side.
      // The geometry reader measures the full text width and triggers the
      // clipping through the size anchor preference.
      GeometryReader { geometry in
        let text = OmniboxText(text, highlightColor: highlightColor)
          .fixedSize(horizontal: true, vertical: true)
          .anchorPreference(key: SizePreferenceKey.self, value: .bounds) { bounds in
            geometry[bounds].size
          }
          .onPreferenceChange(SizePreferenceKey.self) { newSize in
            let textSizeChangedSufficiently =
              abs(textSize.width - newSize.width) > Dimensions.epsilon
              || abs(textSize.height - newSize.height) > Dimensions.epsilon
            if textSizeChangedSufficiently {
              textSize = newSize
            }
          }

        // Wrap the text in a container with a fixed frame. The `text` is rendered
        // at fixedSize inside of it, therefore this acts as a way to clip it.
        let contents = VStack { text }
          .frame(
            width: geometry.size.width,
            alignment: alignment
          )
          // Force LTR layout direction to prevent incorrect behavior in RTL locales.
          // The goal is to deal with the text language, not the user's locale.
          .environment(\.layoutDirection, .leftToRight)

        let truncated = textSize.width > geometry.size.width
        if truncated {
          contents.mask(gradient)
        } else {
          contents
        }
      }
    }.frame(height: textSize.height)
  }
}
