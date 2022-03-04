// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

struct PopupView: View {
  enum Dimensions {
    static let matchListRowInsets = EdgeInsets(.zero)
  }

  @ObservedObject var model: PopupModel
  var body: some View {
    VStack {
      List {
        ForEach(Array(zip(model.matches.indices, model.matches)), id: \.0) {
          (index, match) in
          PopupMatchRowView(
            match: match,
            selectionHandler: {
              model.delegate?.autocompleteResultConsumer(model, didSelectRow: UInt(index))
            },
            trailingButtonHandler: {
              model.delegate?.autocompleteResultConsumer(
                model, didTapTrailingButtonForRow: UInt(index))
            }
          )
          .deleteDisabled(!match.supportsDeletion)
          .listRowInsets(Dimensions.matchListRowInsets)
        }
        .onDelete { indexSet in
          for matchIndex in indexSet {
            model.delegate?.autocompleteResultConsumer(
              model, didSelectRowForDeletion: UInt(matchIndex))
          }
        }
      }
    }
  }
}

struct PopupView_Previews: PreviewProvider {
  static var previews: some View {
    PopupView(
      model: PopupModel(
        matches: PopupMatch.previews, delegate: nil))
  }
}
