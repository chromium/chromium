// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A button view to trigger the pedal/action associated with an Omnibox popup row.
struct PopupMatchRowActionButton: View {
  enum Colors {
    static let foregroundColor = Color.chromeBlue
    static let backgroundColor = Color(red: 0.933, green: 0.933, blue: 0.941)
  }

  enum Constants {
    static let textKerning = -0.17
  }

  enum Dimensions {
    static let backgroundCornerRadius: CGFloat = 49
    static let height: CGFloat = 36
    static let horizontalSpacing: CGFloat = 3
    static let iconPadding = EdgeInsets(top: 0, leading: 0, bottom: 0, trailing: 1)
    static let iconWidth: CGFloat = 17
    static let innerPadding = EdgeInsets(top: 3, leading: 13, bottom: 3, trailing: 12)
    static let textLineHeight: CGFloat = 22
  }

  static let font: UIFont =
    .init(name: "SFProText-Medium", size: 13) ?? .systemFont(ofSize: 13)

  let pedal: OmniboxPedal

  var body: some View {
    Button(action: pedal.action) {
      HStack(spacing: Dimensions.horizontalSpacing) {
        Image(systemName: "clock.arrow.circlepath")
          .resizable()
          .aspectRatio(contentMode: .fit)
          .frame(width: Dimensions.iconWidth)
          .padding(Dimensions.iconPadding)
        Text(pedal.title)
          .font(.init(PopupMatchRowActionButton.font))
          .kerning(Constants.textKerning)

          /// These two line spacing and padding formulae enforce a given line height for a given font.
          .lineSpacing(
            Dimensions.textLineHeight - PopupMatchRowActionButton.font.lineHeight
          )
          .padding(
            .vertical, (Dimensions.height - PopupMatchRowActionButton.font.lineHeight) / 2
          )
      }
      .foregroundColor(Colors.foregroundColor)
      .padding(Dimensions.innerPadding)
      .frame(height: Dimensions.height)
      .background(
        RoundedRectangle(cornerRadius: Dimensions.backgroundCornerRadius)
          .fill(Colors.backgroundColor)
      )
    }
    .buttonStyle(.plain)
  }
}
