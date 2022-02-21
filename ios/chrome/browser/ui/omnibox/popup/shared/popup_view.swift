// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

struct PopupView: View {
  @ObservedObject var model: PopupModel
  var body: some View {
    VStack {
      List {
        ForEach(model.matches) { match in
          PopupMatchRowView(match: match)
            .deleteDisabled(!match.supportsDeletion)
        }
        .onDelete { indexSet in model.matches.remove(atOffsets: indexSet) }
      }
    }
  }
}

struct PopupView_Previews: PreviewProvider {
  static var previews: some View {
    PopupView(
      model: PopupModel(
        matches: PopupMatch.previews))
  }
}
