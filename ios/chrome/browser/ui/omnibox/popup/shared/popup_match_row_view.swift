// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// GradientOverlayRight applies a fading effect as an overlay from left to right.
struct GradientOverlayRight: ViewModifier {
  func body(content: Content) -> some View {
    content
      .frame(maxWidth: .infinity, alignment: .leading)
      .overlay(
        LinearGradient(
          gradient: Gradient(stops: [
            Gradient.Stop(color: .white.opacity((0)), location: 0.9),
            Gradient.Stop(color: .white, location: 1),
          ]),
          startPoint: .leading, endPoint: .trailing)
      )
  }
}

/// GradientOverlayRight applies a fading effect as an overlay from right to left.
struct GradientOverlayLeft: ViewModifier {
  func body(content: Content) -> some View {
    content
      .frame(maxWidth: .infinity, alignment: .leading)
      .overlay(
        LinearGradient(
          gradient: Gradient(stops: [
            Gradient.Stop(color: .white.opacity((0)), location: 0.1),
            Gradient.Stop(color: .white, location: 0),
          ]),
          startPoint: .leading, endPoint: .trailing)
      )
  }
}

/// PreferenceKey to listen to changes of a view's size.
struct SizePreferenceKey: PreferenceKey {
  static var defaultValue = CGSize.zero
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
      rawContent.gradientOverlayRight()
    } else {
      rawContent
    }
  }
}

extension View {
  func truncatedWithGradient() -> some View {
    self.modifier(TruncatedWithGradient())
  }
  func gradientOverlayLeft() -> some View {
    self.modifier(GradientOverlayLeft())
  }
  func gradientOverlayRight() -> some View {
    self.modifier(GradientOverlayRight())
  }
}

struct PopupMatchRowView: View {

  enum Dimensions {
    static let actionButtonOffset = CGSize(width: -5, height: 0)
    static let actionButtonOuterPadding = EdgeInsets(top: 2, leading: 0, bottom: 2, trailing: 0)
    static let leadingSpacing: CGFloat = 60
    static let minHeight: CGFloat = 58
    static let maxHeight: CGFloat = 98
    static let padding = EdgeInsets(top: 9, leading: 0, bottom: 9, trailing: 16)
    static let textHeight: CGFloat = 40
  }

  let match: PopupMatch
  let isHighlighted: Bool
  let selectionHandler: () -> Void
  let trailingButtonHandler: () -> Void

  @State var isPressed = false
  @State var childView = CGSize.zero

  var body: some View {
    ZStack {
      if self.isPressed || self.isHighlighted { Color.cr_tableRowViewHighlight }

      Button(action: selectionHandler) { Rectangle().fill(.clear).contentShape(Rectangle()) }
        .buttonStyle(PressedPreferenceKeyButtonStyle())
        .onPreferenceChange(PressedPreferenceKey.self) { isPressed in
          self.isPressed = isPressed
        }

      /// The content is in front of the button, for proper hit testing.
      HStack(alignment: .center, spacing: 0) {
        HStack(alignment: .center, spacing: 0) {
          Spacer()
          match.image.map { image in PopupMatchImageView(image: image) }
          Spacer()
        }.frame(width: Dimensions.leadingSpacing)
        VStack(alignment: .leading, spacing: 0) {
          VStack(alignment: .leading, spacing: 0) {
            Text(match.text)
              .lineLimit(1)
              .truncatedWithGradient()

            if let subtitle = match.detailText, !subtitle.isEmpty {
              Text(subtitle)
                .font(.footnote)
                .foregroundColor(Color.gray)
                .lineLimit(1)
                .truncatedWithGradient()
            }
          }
          .frame(height: Dimensions.textHeight)
          .allowsHitTesting(false)

          if let pedal = match.pedal {
            PopupMatchRowActionButton(pedal: pedal)
              .padding(Dimensions.actionButtonOuterPadding)
              .offset(Dimensions.actionButtonOffset)
          }
        }
        Spacer()
        if match.isAppendable || match.isTabMatch {
          PopupMatchTrailingButton(match: match, action: trailingButtonHandler)
        }
      }
      .padding(Dimensions.padding)
    }
    .frame(maxWidth: .infinity, minHeight: Dimensions.minHeight, maxHeight: Dimensions.maxHeight)
  }
}

struct PopupMatchRowView_Previews: PreviewProvider {
  static var previews: some View = PopupView_Previews.previews
}
