// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A button view to act as the trailing button of a popup match and provide an additional action for a match.
struct PopupMatchTrailingButton: View {
  enum Colors {
    static let trailingButtonColor = Color(red: 0.769, green: 0.769, blue: 0.769)
  }

  enum Dimensions {
    static let extendedTouchTargetDiameter: CGFloat = 44
    static let trailingButtonSize: CGFloat = 24
  }

  let match: PopupMatch
  let action: () -> Void

  var body: some View {
    Button(action: action) {
      Image(systemName: match.isAppendable ? "arrow.up.backward" : "arrow.right.square")
        .foregroundColor(Colors.trailingButtonColor)
        .aspectRatio(contentMode: .fit)
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
    .buttonStyle(.plain)
  }
}
