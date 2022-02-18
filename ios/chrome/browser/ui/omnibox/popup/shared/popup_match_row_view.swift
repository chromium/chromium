// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

struct PopupMatchRowView: View {
  enum Colors {
    static let trailingButtonColor = Color(hue: 0, saturation: 0, brightness: 0.77)
  }

  enum Dimensions {
    static let tailButtonSize: CGFloat = 24
    static let extendedTouchTargetDiameter: CGFloat = 44
  }

  let match: PopupMatch

  var body: some View {
    HStack {
      VStack(alignment: .leading, spacing: 4.0) {
        Text(match.title)
          .lineLimit(1)

        if let subtitle = match.subtitle {
          Text(subtitle)
            .font(.footnote)
            .foregroundColor(Color.gray)
            .lineLimit(1)
        }
        if let pedal = match.pedal {
          Button(pedal.title) {
            print("Pressed")
          }
          .buttonStyle(BorderlessButtonStyle())
        }
      }
      if match.isAppendable || match.isTabMatch {
        Spacer()
        Button(action: match.trailingButtonTapped) {
          Image(systemName: match.isAppendable ? "arrow.up.backward" : "arrow.right.square")
            .foregroundColor(Colors.trailingButtonColor)
        }
        .frame(
          width: Dimensions.tailButtonSize, height: Dimensions.tailButtonSize,
          alignment: .center
        )
        .contentShape(
          Circle().size(
            width: Dimensions.extendedTouchTargetDiameter,
            height: Dimensions.extendedTouchTargetDiameter)
        )
        .buttonStyle(BorderlessButtonStyle())
      }
    }
  }

}
