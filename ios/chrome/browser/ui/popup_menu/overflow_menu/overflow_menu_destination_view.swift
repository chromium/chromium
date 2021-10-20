// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// A view displaying a single destination.
struct OverflowMenuDestinationView: View {

  enum Dimensions {
    /// The size of the total icon, including the background.
    static let iconSize: CGFloat = 60
    static let cornerRadius: CGFloat = 13
    /// The padding on either side of the text, separating it from the next view.
    static let textPadding: CGFloat = 3
  }

  /// The destination for this view.
  var destination: OverflowMenuDestination

  /// The spacing to either side of each icon.
  var iconSpacing: CGFloat

  var body: some View {
    VStack {
      ZStack(alignment: .center) {
        Rectangle()
          .foregroundColor(.cr_groupedSecondaryBackground)
          .frame(width: Dimensions.iconSize, height: Dimensions.iconSize)
          .cornerRadius(Dimensions.cornerRadius)
        destination.image
      }
      .padding([.leading, .trailing], iconSpacing)

      Text(destination.name)
        .font(.caption2)
        .padding([.leading, .trailing], Dimensions.textPadding)
    }
    .contentShape(Rectangle())
    .onTapGesture(perform: destination.handler)
  }
}
