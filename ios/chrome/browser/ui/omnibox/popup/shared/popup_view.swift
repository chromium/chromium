// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Utility which provides a way to treat the `simultaneousGesture` view modifier as a value.
struct SimultaneousGestureModifier<T: Gesture>: ViewModifier {
  let gesture: T
  let mask: GestureMask

  init(_ gesture: T, including mask: GestureMask = .all) {
    self.gesture = gesture
    self.mask = mask
  }

  func body(content: Content) -> some View {
    content.simultaneousGesture(gesture, including: mask)
  }
}

/// A view modifier which embeds the content in a `ScrollViewReader` and calls `action`
/// when the provided `value` changes. It is similar to the `onChange` view modifier, but provides
/// a `ScrollViewProxy` object in addition to the new state of `value` when calling `action`.
struct ScrollOnChangeModifier<V: Equatable>: ViewModifier {
  @Binding var value: V
  let action: (V, ScrollViewProxy) -> Void
  func body(content: Content) -> some View {
    ScrollViewReader { scrollProxy in
      content.onChange(of: value) { newState in action(newState, scrollProxy) }
    }
  }
}

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

  var listContent: some View {
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

  @ViewBuilder
  var listView: some View {
    let listModifier = SimultaneousGestureModifier(DragGesture().onChanged { onDrag($0) })
      .concat(ScrollOnChangeModifier(value: $model.sections, action: onNewMatches))
      .concat(BlurredBackground())
    if shouldSelfSize {
      SelfSizingList(
        bottomMargin: Dimensions.selfSizingListBottomMargin,
        listModifier: listModifier,
        content: {
          listContent
        },
        emptySpace: {
          PopupEmptySpaceView()
        }
      )
    } else {
      List {
        listContent
      }
      .modifier(listModifier)
      .ignoresSafeArea(.keyboard)
    }
  }

  var body: some View {
    listView.onAppear(perform: onAppear)
  }

  func onAppear() {
    if let appearanceContainerType = self.appearanceContainerType {
      let listAppearance = UITableView.appearance(whenContainedInInstancesOf: [
        appearanceContainerType
      ])
      listAppearance.backgroundColor = .clear

      if shouldSelfSize {
        listAppearance.bounces = false
      }
    }
  }

  func onDrag(_ dragValue: DragGesture.Value) {
    model.highlightedMatchIndexPath = nil
    model.delegate?.autocompleteResultConsumerDidScroll(model)
  }

  func onNewMatches(matches: [PopupMatchSection], scrollProxy: ScrollViewProxy) {
    // Scroll to the very top of the list.
    scrollProxy.scrollTo(0, anchor: UnitPoint(x: 0, y: -.infinity))
  }
}

struct PopupView_Previews: PreviewProvider {
  static var previews: some View {
    PopupView(
      model: PopupModel(
        matches: [PopupMatch.previews], headers: ["Suggestions"], delegate: nil))
  }
}
