// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

struct PopupView: View {
  enum Dimensions {
    static let tailButtonSize: CGFloat = 24
    static let extendedTouchTargetDiameter: CGFloat = 44
  }

  @ObservedObject var model: PopupModel
  var body: some View {
    VStack {
      List(model.matches) { match in
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
            Button(action: { model.trailingButtonHandler(match) }) {
              Image(systemName: match.isAppendable ? "arrow.up.backward" : "arrow.right.square")
            }
            .foregroundColor(.gray)
            .frame(
              width: Dimensions.tailButtonSize, height: Dimensions.tailButtonSize,
              alignment: .center
            )
            .contentShape(
              Circle().size(
                width: Dimensions.extendedTouchTargetDiameter,
                height: Dimensions.extendedTouchTargetDiameter)
            )
            .buttonStyle(PlainButtonStyle())
          }
        }
      }
      Button("Add matches") {
        model.buttonHandler()
      }
    }
  }
}

struct PopupView_Previews: PreviewProvider {
  static var previews: some View {
    PopupView(
      model: PopupModel(
        matches: PopupMatch.previews, buttonHandler: {},
        trailingButtonHandler: { match in print(match) }))
  }
}
