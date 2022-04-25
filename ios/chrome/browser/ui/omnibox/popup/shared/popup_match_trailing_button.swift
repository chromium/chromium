// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A button view to act as the trailing button of a popup match and provide an additional action for a match.
struct PopupMatchTrailingButton: View {
  enum Dimensions {
    static let extendedTouchTargetDiameter: CGFloat = 44
    static let trailingButtonSize: CGFloat = 24
  }

  let match: PopupMatch
  let action: () -> Void

  var body: some View {
    Button(action: action) {
      Image(systemName: match.isTabMatch ? "arrow.right.square" : "arrow.up.backward")
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
    // The button shouldn't be an actual accessibility element for
    // VoiceOver.
    .accessibilityHidden(true)
    // TODO(crbug.com/1312110): This should be `children: .contain` so the
    // new accessibility element isn't accessible. However, EG currently can't
    // tap on a non-accessible SwiftUI view in a test.
    // Create a new accessibility element that is non-accessible so tests
    // can find the button.
    .accessibilityElement(children: .ignore)
    .accessibilityIdentifier(
      match.isTabMatch
        ? kOmniboxPopupRowSwitchTabAccessibilityIdentifier
        : kOmniboxPopupRowAppendAccessibilityIdentifier)
  }
}
