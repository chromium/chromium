// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// The circular edit button allowing users to show or hide individual
/// destinations during customization.
struct DestinationEditButton: View {
  let destination: OverflowMenuDestination

  var iconName: String {
    return destination.shown ? "minus" : "plus"
  }

  var buttonColor: Color {
    return destination.shown ? .gray : .green
  }

  @ViewBuilder
  var body: some View {
    Button {
      withAnimation {
        destination.shown.toggle()
      }
    } label: {
      ZStack(alignment: .center) {
        Circle().fill()
        Image(systemName: iconName).blendMode(.destinationOut)
      }
      .compositingGroup()
      .foregroundColor(buttonColor)
      .frame(width: 20, height: 20)
    }
  }
}
