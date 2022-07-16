// Copyright 2021 The Chromium Authors. All rights reserved.
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
          .foregroundColor(.cr_grey300)
          .frame(height: Dimensions.iconLineDividerHeight)
        footer.image
          .fixedSize()
          .foregroundColor(.cr_grey300)
        Rectangle()
          .foregroundColor(.cr_grey300)
          .frame(height: Dimensions.iconLineDividerHeight)
      }
      .padding([.top], Dimensions.iconLineExtraVerticalSpacing)
      // Add empty tap gesture to the non tapable text, otherwise the link
      // onTapGesture is triggered when tapping on this text too.
      Text(footer.name)
        .padding([.top], Dimensions.iconLineExtraVerticalSpacing)
        .font(.footnote)
        .onTapGesture {}
      Text(footer.link)
        .font(.caption2)
        .foregroundColor(.cr_blue)
        .onTapGesture(perform: footer.handler)
    }
    .contentShape(Rectangle())
  }
}
