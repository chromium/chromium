// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A button view to act as the trailing button of a popup match and provide an additional action for a match.
struct PopupMatchTrailingButton: View {
  enum Dimensions {
    static let extendedTouchTargetDiameter: CGFloat = 44
    static let trailingButtonIconSize: CGFloat = 17
    static let trailingButtonSize: CGFloat = 24
  }

  let match: PopupMatch
  let action: () -> Void

  @Environment(\.popupUIVariation) var uiVariation: PopupUIVariation
  @Environment(\.layoutDirection) var layoutDirection: LayoutDirection

  @ViewBuilder
  var image: some View {
    switch uiVariation {
    case .one:
      // Treatment one uses legacy icons.
      if match.isTabMatch {
        let uiImage = UIImage(named: "omnibox_popup_tab_match")
        Image(uiImage: uiImage!)
          .renderingMode(.template)
          .flipsForRightToLeftLayoutDirection(true)
      } else {
        let uiImage = NativeImage(IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND)
        Image(uiImage: uiImage!)
          .renderingMode(.template)
          .flipsForRightToLeftLayoutDirection(true)
      }
    case .two:
      Image(systemName: match.isTabMatch ? "arrow.right.circle" : "arrow.up.backward")
        .flipsForRightToLeftLayoutDirection(true)
    }
  }

  var body: some View {
    Button(action: action) {
      image
        // Make the image know about the environment layout direction as we
        // override it on the body as a whole.
        .environment(\.layoutDirection, layoutDirection)
        .font(.system(size: Dimensions.trailingButtonIconSize, weight: .medium))
        .aspectRatio(contentMode: .fit)
        .frame(
          width: Dimensions.trailingButtonSize, height: Dimensions.trailingButtonSize,
          alignment: .center
        )
        .contentShape(
          Circle().size(
            width: Dimensions.extendedTouchTargetDiameter,
            height: Dimensions.extendedTouchTargetDiameter
          )
          .offset(
            x: (Dimensions.trailingButtonSize - Dimensions.extendedTouchTargetDiameter) / 2,
            y: (Dimensions.trailingButtonSize - Dimensions.extendedTouchTargetDiameter) / 2)
        )
    }
    .buttonStyle(.plain)
    // The button shouldn't be an actual accessibility element for
    // VoiceOver.
    .accessibilityHidden(true)
    .accessibilityIdentifier(
      match.isTabMatch
        ? kOmniboxPopupRowSwitchTabAccessibilityIdentifier
        : kOmniboxPopupRowAppendAccessibilityIdentifier
    )
    .environment(\.layoutDirection, .leftToRight)
  }
}
