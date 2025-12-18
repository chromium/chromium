// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A view that displays a footer in the overflow menu.
struct OverflowMenuFooterRow: View {

  enum Dimensions {
    /// Space between elements on the icon line.
    static let iconLineSpacing: CGFloat = 12
    /// The vertical spacing between icon line, label (name) and link.
    static let verticalSpacing: CGFloat = 3
    /// The extra vertical above and below icon line.
    static let iconLineExtraVerticalSpacing: CGFloat = 9
    // The height of the icon line dividers.
    static let iconLineDividerHeight: CGFloat = 1
  }

  /// The footer for this row.
  @ObservedObject var footer: OverflowMenuFooter

  var body: some View {
    VStack(
      alignment: .center,
      spacing: Dimensions.verticalSpacing
    ) {
      HStack(
        alignment: .center,
        spacing: Dimensions.iconLineSpacing
      ) {
        Rectangle()
          .foregroundColor(.overflowMenuSeparator)
          .frame(height: Dimensions.iconLineDividerHeight)
        footer.image
          .fixedSize()
          .foregroundColor(.grey500)
        Rectangle()
          .foregroundColor(.overflowMenuSeparator)
          .frame(height: Dimensions.iconLineDividerHeight)
      }
      .padding([.top], Dimensions.iconLineExtraVerticalSpacing)
      Text(footer.name)
        .padding([.top], Dimensions.iconLineExtraVerticalSpacing)
        .font(.footnote)
      Text(footer.link)
        .font(.caption2)
        .foregroundColor(.chromeBlue)
        .onTapGesture(perform: footer.handler)
        .accessibilityIdentifier(footer.accessibilityIdentifier)
    }
    // Group all children together so VoiceOver doesn't have to read the two
    // text labels individually.
    .accessibilityElement(children: .combine)
    .accessibilityAction(.default, footer.handler)
  }
}
