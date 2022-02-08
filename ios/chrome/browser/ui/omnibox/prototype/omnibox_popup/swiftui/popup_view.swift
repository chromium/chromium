// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

struct PopupView: View {
  @ObservedObject var model: PopupModel
  var body: some View {
    VStack {
      List(model.matches) { match in
        VStack(alignment: .leading, spacing: 4.0) {
          Text(match.title)
            .lineLimit(1)
          Text(match.subtitle)
            .font(.footnote)
            .foregroundColor(Color.gray)
            .lineLimit(1)
          if let pedal = match.pedal {
            // match.pedal is non-nil
            Button(pedal.title) {
              print("Pressed")
            }
            .buttonStyle(BorderlessButtonStyle())
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
    PopupView(model: PopupModel(matches: PopupMatch.previews, buttonHandler: {}))
  }
}
