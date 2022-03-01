// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

struct PopupMatchRowView: View {
  enum Constants {
    static let actionButtonTextKerning = -0.17
  }

  enum Colors {
    static let actionButtonForegroundColor = Color.cr_blue
    static let actionButtonBackgroundColor = Color(red: 0.933, green: 0.933, blue: 0.941)
    static let trailingButtonColor = Color(red: 0.769, green: 0.769, blue: 0.769)
  }

  enum Dimensions {
    static let actionButtonOffset = CGSize(width: -5, height: 0)
    static let actionButtonOuterPadding = EdgeInsets(top: 2, leading: 0, bottom: 2, trailing: 0)
    static let extendedTouchTargetDiameter: CGFloat = 44
    static let leadingSpacing: CGFloat = 60
    static let minHeight: CGFloat = 58
    static let maxHeight: CGFloat = 98
    static let textHeight: CGFloat = 40
    static let trailingButtonSize: CGFloat = 24
  }

  let match: PopupMatch

  var body: some View {
    HStack {
      Spacer().frame(width: Dimensions.leadingSpacing)
      VStack(alignment: .leading, spacing: 0) {
        VStack(alignment: .leading, spacing: 0) {
          Text(match.title)
            .lineLimit(1)

          if let subtitle = match.subtitle {
            Text(subtitle)
              .font(.footnote)
              .foregroundColor(Color.gray)
              .lineLimit(1)
          }
        }
        .frame(height: Dimensions.textHeight)

        if let pedal = match.pedal {
          PopupMatchRowActionButton(pedal: pedal)
            .padding(Dimensions.actionButtonOuterPadding)
            .offset(Dimensions.actionButtonOffset)
        }
      }
      if match.isAppendable || match.isTabMatch {
        Spacer()
        Button(action: match.trailingButtonTapped) {
          Image(systemName: match.isAppendable ? "arrow.up.backward" : "arrow.right.square")
            .foregroundColor(Colors.trailingButtonColor)
        }
        .buttonStyle(.plain)
        .frame(
          width: Dimensions.trailingButtonSize, height: Dimensions.trailingButtonSize,
          alignment: .center
        )
        .contentShape(
          Circle().size(
            width: Dimensions.extendedTouchTargetDiameter,
            height: Dimensions.extendedTouchTargetDiameter)
        )
      }
    }
    .frame(minHeight: Dimensions.minHeight, maxHeight: Dimensions.maxHeight)
  }

}

struct PopupMatchRowView_Previews: PreviewProvider {
  static var previews: some View = PopupView_Previews.previews
}
