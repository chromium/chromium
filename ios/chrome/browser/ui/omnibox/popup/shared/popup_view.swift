// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

struct PopupView: View {
  enum Dimensions {
    static let matchListRowInsets = EdgeInsets(.zero)
    static let selfSizingListBottomMargin: CGFloat = 40
  }

  @ObservedObject var model: PopupModel
  private let shouldSelfSize: Bool
  private let appearanceContainerType: UIAppearanceContainer.Type?

  init(
    model: PopupModel, shouldSelfSize: Bool = false,
    appearanceContainerType: UIAppearanceContainer.Type? = nil
  ) {
    self.model = model
    self.shouldSelfSize = shouldSelfSize
    self.appearanceContainerType = appearanceContainerType
  }

  var content: some View {
    ForEach(Array(zip(model.sections.indices, model.sections)), id: \.0) {
      sectionIndex, section in

      let sectionContents =
        ForEach(Array(zip(section.matches.indices, section.matches)), id: \.0) {
          matchIndex, match in
          PopupMatchRowView(
            match: match,
            isHighlighted: IndexPath(row: matchIndex, section: sectionIndex)
              == self.model.highlightedMatchIndexPath,

            selectionHandler: {
              model.delegate?.autocompleteResultConsumer(
                model, didSelectRow: UInt(matchIndex), inSection: UInt(sectionIndex))
            },
            trailingButtonHandler: {
              model.delegate?.autocompleteResultConsumer(
                model, didTapTrailingButtonForRow: UInt(matchIndex),
                inSection: UInt(sectionIndex))
            }
          )
          .deleteDisabled(!match.supportsDeletion)
          .listRowInsets(Dimensions.matchListRowInsets)
        }
        .onDelete { indexSet in
          for matchIndex in indexSet {
            model.delegate?.autocompleteResultConsumer(
              model, didSelectRowForDeletion: UInt(matchIndex), inSection: UInt(sectionIndex))
          }
        }

      // Split the suggestions into sections, but only add a header text if the header isn't empty
      if !section.header.isEmpty {
        Section(header: Text(section.header)) {
          sectionContents
        }
      } else {
        Section {
          sectionContents
        }
      }
    }
  }

  var body: some View {
    if shouldSelfSize {
      SelfSizingList(bottomMargin: Dimensions.selfSizingListBottomMargin) {
        content
      } emptySpace: {
        PopupEmptySpaceView()
      }
      .onAppear(perform: onAppear)
    } else {
      List {
        content
      }
      .onAppear(perform: onAppear)
    }
  }

  func onAppear() {
    if let appearanceContainerType = self.appearanceContainerType {
      let listAppearance = UITableView.appearance(whenContainedInInstancesOf: [
        appearanceContainerType
      ])

      listAppearance.bounces = false
    }
  }
}

struct PopupView_Previews: PreviewProvider {
  static var previews: some View {
    PopupView(
      model: PopupModel(
        matches: [PopupMatch.previews], headers: ["Suggestions"], delegate: nil))
  }
}
