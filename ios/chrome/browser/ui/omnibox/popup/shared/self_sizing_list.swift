// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A preference key which is meant to compute the greatest `maxY` value of multiple elements.
struct MaxYPreferenceKey: PreferenceKey {
  static var defaultValue: CGFloat? = nil
  static func reduce(value: inout CGFloat?, nextValue: () -> CGFloat?) {
    if let nextValue = nextValue() {
      if let currentValue = value {
        value = max(currentValue, nextValue)
      } else {
        value = nextValue
      }
    }
  }
}

/// A preference key to return the height of a `SelfSizingList`.
struct SelfSizingListHeightPreferenceKey: PreferenceKey {
  static var defaultValue: CGFloat? = nil
  static func reduce(value: inout CGFloat?, nextValue: () -> CGFloat?) {
    if let nextValue = nextValue() {
      if let currentValue = value {
        value = max(currentValue, nextValue)
      } else {
        value = nextValue
      }
    }
  }
}

/// View which acts like a `List` but which also clips whatever empty space
/// is available below the actual content to replace it with an arbitrary view.
struct SelfSizingList<Content: View, EmptySpace: View, ListModifier: ViewModifier>: View {

  let bottomMargin: CGFloat
  var listModifier: ListModifier
  var content: () -> Content
  var emptySpace: () -> EmptySpace

  /// - Parameters:
  ///   - bottomMargin: The bottom margin below the end of the list.
  ///   - listModifier: A `ViewModifier` to apply to the internal list.
  ///   - content: The content of the list.
  ///   - emptySpace: The view used to replace the area below the list.
  init(
    bottomMargin: CGFloat = 0,
    listModifier: ListModifier,
    @ViewBuilder content: @escaping () -> Content,
    @ViewBuilder emptySpace: @escaping () -> EmptySpace
  ) {
    self.bottomMargin = bottomMargin
    self.listModifier = listModifier
    self.content = content
    self.emptySpace = emptySpace
  }

  @State private var contentHeightEstimate: CGFloat? = nil

  var body: some View {
    GeometryReader { geometry in
      let availableHeight = geometry.size.height

      VStack(spacing: 0) {
        VStack(spacing: 0) {
          List {
            content()
              .anchorPreference(key: MaxYPreferenceKey.self, value: .bounds) { geometry[$0].maxY }
          }
          .modifier(listModifier)
          .onPreferenceChange(MaxYPreferenceKey.self) { newMaxY in
            let newContentHeightEstimate = newMaxY.map {
              min($0 + bottomMargin, geometry.size.height)
            }
            if self.contentHeightEstimate != newContentHeightEstimate {
              self.contentHeightEstimate = newContentHeightEstimate
            }
          }
          .frame(height: availableHeight, alignment: .top)
        }
        .preference(key: SelfSizingListHeightPreferenceKey.self, value: self.contentHeightEstimate)
        .frame(height: self.contentHeightEstimate, alignment: .top)
        .clipped()

        // Empty space view to identify and explicitly ignore during hit testing.
        emptySpace()
      }
    }
  }
}
