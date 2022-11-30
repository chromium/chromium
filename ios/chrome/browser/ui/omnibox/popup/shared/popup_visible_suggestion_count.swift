// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

extension View {
  // Read the size of the view and calls `onChange` with the new size, when
  // the size changes.
  func notifyOnSizeChange(_ onChange: @escaping (CGSize) -> Void) -> some View {
    background(
      GeometryReader { geometryProxy in
        Color.clear
          .preference(key: SizePreferenceKey.self, value: geometryProxy.size)
      }
    )
    .onPreferenceChange(SizePreferenceKey.self, perform: onChange)
  }
}

// Handles visible suggestions count measured in `popupView`.
// Adds a hidden fake suggestion cell behind the view to measure the height of a
// typical suggestion row.
// Adds a Spacer() behind the view to measure the available height of the view.
// Updates `visibleSuggestionCount` of `popupModel`.
struct VisibleSuggestionCountModifier: ViewModifier {

  weak var model: PopupModel?
  @ObservedObject var uiConfiguration: PopupUIConfiguration

  @State private var fakeCellHeight: CGFloat = PopupMatchRowView.Dimensions.minHeight
  @State private var visibleHeight: CGFloat? = nil

  init(model: PopupModel, uiConfiguration: PopupUIConfiguration) {
    self.model = model
    self.uiConfiguration = uiConfiguration
  }

  func body(content: Content) -> some View {
    ZStack {
      // Spacer to read the space between the omnibox and the keyboard.
      Spacer()
        .notifyOnSizeChange { spacerSize in
          self.visibleHeight = spacerSize.height
          updateVisibleSuggestionCount()
        }
        .hidden()
      // fakeSuggestion to read the size of a suggestion row.
      fakeSuggestion
      content
    }

  }

  var fakeSuggestion: some View {
    // A match that has the most common display properties.
    List {
      Section {
        let typicalMatch = PopupMatch(
          suggestion: PopupMatch.FakeAutocompleteSuggestion(
            text: "Fake suggestion",
            icon: FakeOmniboxIcon.suggestionIcon))
        PopupMatchRowView(
          match: typicalMatch,
          isHighlighted: false,
          toolbarConfiguration: uiConfiguration.toolbarConfiguration,
          selectionHandler: {},
          trailingButtonHandler: {},
          uiConfiguration: uiConfiguration,
          shouldDisplayCustomSeparator: false
        )
        .listRowInsets(EdgeInsets())
        .hidden()
        .onPreferenceChange(
          PopupMatchRowSizePreferenceKey.self,
          perform: { cellSize in
            if cellSize.height != fakeCellHeight {
              fakeCellHeight = cellSize.height
              updateVisibleSuggestionCount()
            }
          })
      }
    }
  }

  func updateVisibleSuggestionCount() {
    if fakeCellHeight == 0 || visibleHeight == nil {
      return
    }
    let visibleCellCount = self.visibleHeight! / fakeCellHeight
    self.model?.visibleSuggestionCount = UInt(max(0, floor(visibleCellCount)))
  }
}

extension View {
  // Apply VisibleSuggestionCountModifier to the target view.
  func measureVisibleSuggestionCount(
    with configuration: PopupUIConfiguration,
    updating model: PopupModel
  )
    -> some View
  {
    modifier(
      VisibleSuggestionCountModifier(
        model: model,
        uiConfiguration: configuration))
  }
}
