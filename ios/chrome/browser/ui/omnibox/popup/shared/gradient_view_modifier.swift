// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// GradientOverlay applies a fading effect from clear to opaque as an overlay
/// heading in `direction`. E.g. `direction = .right` means the fade goes from
/// left to right.
struct GradientOverlay: ViewModifier {
  enum OverlayDirection {
    case right
    case left
  }

  var direction: OverlayDirection

  func body(content: Content) -> some View {
    content
      .frame(maxWidth: .infinity, alignment: .leading)
      .mask(
        LinearGradient(
          gradient: Gradient(stops: stops),
          startPoint: .leading, endPoint: .trailing)
      )
  }

  var stops: [Gradient.Stop] {
    switch direction {
    case .right:
      return [
        Gradient.Stop(color: .white, location: 0.9),
        Gradient.Stop(color: .clear, location: 1),
      ]
    case .left:
      return [
        Gradient.Stop(color: .white, location: 0.1),
        Gradient.Stop(color: .clear, location: 0),
      ]
    }
  }
}

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

/// A view modifier that applies a transparent gradient effect when the content
/// is too wide to fit in its container. The content is clipped on the trailing
/// edge.
struct TruncatedWithGradient: ViewModifier {

  // Child view size (width,height)
  @State var childSize = CGSize.zero

  // Widths comparison tracker
  @State var truncated = false

  func body(content: Content) -> some View {
    let rawContent = GeometryReader { outerGeo in
      content
        .fixedSize(horizontal: true, vertical: true)
        .overlay(
          GeometryReader { innerGeo in
            Rectangle()
              .hidden()
              .preference(key: SizePreferenceKey.self, value: innerGeo.size)
              .onPreferenceChange(SizePreferenceKey.self) {
                newSize in
                childSize = newSize
                truncated = childSize.width > outerGeo.size.width
              }
          }
        )
    }
    .frame(height: childSize.height)
    .clipped()

    if truncated {
      rawContent.gradientOverlay(direction: .right)
    } else {
      rawContent
    }
  }
}

extension View {
  func truncatedWithGradient() -> some View {
    self.modifier(TruncatedWithGradient())
  }
  func gradientOverlay(direction: GradientOverlay.OverlayDirection)
    -> some View
  {
    self.modifier(GradientOverlay(direction: direction))
  }
}
