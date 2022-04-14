// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A preference key which is meant to compute the minimum `minY` value of multiple elements.
struct MinYPreferenceKey: PreferenceKey {
  static var defaultValue: CGFloat? = nil
  static func reduce(value: inout CGFloat?, nextValue: () -> CGFloat?) {
    if let nextValue = nextValue() {
      if let currentValue = value {
        value = min(currentValue, nextValue)
      } else {
        value = nextValue
      }
    }
  }
}

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

/// Utility which provides a way to treat the `accessibilityIdentifier` view modifier as a value.
struct AccessibilityIdentifierModifier: ViewModifier {
  let identifier: String
  func body(content: Content) -> some View {
    content.accessibilityIdentifier(identifier)
  }
}

struct PopupView: View {
  enum Dimensions {
    static let matchListRowInsets = EdgeInsets(.zero)
    // Any scrolling offset outside of this range is not treated as user drag gesture.
    static let acceptableScrollingOffsetDeltaRange = 1.5...10
    // If distance between current and initial scrolling offset is less than this value,
    // it is probably an automatic scroll-to-top so we do not consider it to be user drag gesture.
    static let initialScrollingOffsetDifferenceThreshold: CGFloat = 0.1

    static let selfSizingListBottomMargin: CGFloat = 16

    // Default height if no other header or footer. This spaces the sections
    // out properly.
    static let headerFooterHeight: CGFloat = 16

    // Default list row insets. These are removed to inset the popup to the
    // width of the omnibox
    static let defaultInset: CGFloat = 16
  }

  /// Custom modifier emulating `.environment(key, value)`.
  struct EnvironmentValueModifier<Value>: ViewModifier {
    let key: WritableKeyPath<SwiftUI.EnvironmentValues, Value>
    let value: Value
    init(_ key: WritableKeyPath<SwiftUI.EnvironmentValues, Value>, _ value: Value) {
      self.key = key
      self.value = value
    }
    func body(content: Content) -> some View {
      content.environment(key, value)
    }
  }

  /// Custom modifier emulating `.padding(edges, length)`.
  struct PaddingModifier: ViewModifier {
    let edges: Edge.Set
    let length: CGFloat?
    init(_ edges: Edge.Set = .all, _ length: CGFloat? = nil) {
      self.edges = edges
      self.length = length
    }
    func body(content: Content) -> some View {
      content.padding(edges, length)
    }
  }

  @ObservedObject var model: PopupModel
  private let shouldSelfSize: Bool
  private let appearanceContainerType: UIAppearanceContainer.Type?

  @Environment(\.popupUIVariation) var popupUIVariation: PopupUIVariation

  /// Memory of scrolling offset, so as to be able to tell the difference between user drag gesture and automatic scroll-to-top on new matches
  @State private var initialScrollingOffset: CGFloat? = nil
  @State private var scrollingOffset: CGFloat? = nil

  /// The current height of the self-sizing list.
  @State private var selfSizingListHeight: CGFloat? = nil

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
          let indexPath = IndexPath(row: matchIndex, section: sectionIndex)

          PopupMatchRowView(
            match: match,
            isHighlighted: indexPath == self.model.highlightedMatchIndexPath,
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
          .id(indexPath)
          .deleteDisabled(!match.supportsDeletion)
          .listRowInsets(Dimensions.matchListRowInsets)
          .accessibilityElement(children: .contain)
          .accessibilityIdentifier(
            OmniboxPopupAccessibilityIdentifierHelper.accessibilityIdentifierForRow(at: indexPath))
        }
        .onDelete { indexSet in
          for matchIndex in indexSet {
            model.delegate?.autocompleteResultConsumer(
              model, didSelectRowForDeletion: UInt(matchIndex), inSection: UInt(sectionIndex))
          }
        }

      let footer = Spacer()
        // Use `leastNonzeroMagnitude` to remove the footer. Otherwise,
        // it uses a default height.
        .frame(height: CGFloat.leastNonzeroMagnitude)
        .listRowInsets(EdgeInsets())

      Section(header: header(for: section), footer: footer) {
        sectionContents
      }
    }
  }

  @ViewBuilder
  var listView: some View {
    let commonListModifier = AccessibilityIdentifierModifier(
      identifier: kOmniboxPopupTableViewAccessibilityIdentifier
    )
    .concat(ScrollOnChangeModifier(value: $model.sections, action: onNewSections))
    .concat(EnvironmentValueModifier(\.defaultMinListHeaderHeight, 0))

    GeometryReader { geometry in
      if shouldSelfSize {
        let selfSizingListModifier =
          commonListModifier
          .concat(omniboxPaddingModifier)
        ZStack(alignment: .top) {
          listBackground.frame(height: selfSizingListHeight)
          SelfSizingList(
            bottomMargin: Dimensions.selfSizingListBottomMargin,
            listModifier: selfSizingListModifier,
            content: {
              listContent
                .anchorPreference(key: MinYPreferenceKey.self, value: .bounds) { geometry[$0].minY }
            },
            emptySpace: {
              PopupEmptySpaceView()
            }
          ).frame(width: geometry.size.width, height: geometry.size.height)
            .onPreferenceChange(SelfSizingListHeightPreferenceKey.self) { height in
              selfSizingListHeight = height
            }
        }
      } else {
        List {
          listContent
            .anchorPreference(key: MinYPreferenceKey.self, value: .bounds) { geometry[$0].minY }
        }
        .background(listBackground)
        .modifier(commonListModifier)
        .ignoresSafeArea(.keyboard)
        .frame(width: geometry.size.width, height: geometry.size.height)
      }
    }
    .onPreferenceChange(MinYPreferenceKey.self) { newScrollingOffset in
      if let newScrollingOffset = newScrollingOffset {
        onNewScrollingOffset(
          oldScrollingOffset: scrollingOffset, newScrollingOffset: newScrollingOffset)
      }
    }
  }

  var body: some View {
    listView.onAppear(perform: onAppear)
  }

  @ViewBuilder
  func header(for section: PopupMatchSection) -> some View {
    if section.header.isEmpty {
      Spacer()
        .frame(height: Dimensions.headerFooterHeight)
        .listRowInsets(EdgeInsets())
    } else {
      Text(section.header)
    }
  }

  /// Returns a `ViewModifier` to correctly space the sides of the list based
  /// on the current omnibox spacing
  var omniboxPaddingModifier: some ViewModifier {
    let leadingSpace: CGFloat
    let trailingSpace: CGFloat
    switch popupUIVariation {
    case .one:
      leadingSpace = 0
      trailingSpace = 0
    case .two:
      leadingSpace = model.omniboxLeadingSpace - Dimensions.defaultInset
      trailingSpace = model.omniboxTrailingSpace - Dimensions.defaultInset
    }
    return PaddingModifier([.leading], leadingSpace).concat(
      PaddingModifier([.trailing], trailingSpace))
  }

  var listBackground: some View {
    // iOS 14 + SwiftUI has a bug with dark mode colors in high contrast mode.
    // If no dark mode + high contrast color is provided, they are supposed to
    // default to the dark mode color. Instead, SwiftUI defaults to the light
    // mode color. This bug is fixed in iOS 15, but until then, a workaround
    // color with the dark mode + high contrast color specified is used.
    if #available(iOS 15, *) {
      return Color.cr_groupedSecondaryBackground.edgesIgnoringSafeArea(.all)
    } else {
      return Color("grouped_primary_background_color_swiftui_ios14").edgesIgnoringSafeArea(.all)
    }
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

  func onScroll() {
    model.highlightedMatchIndexPath = nil
    model.delegate?.autocompleteResultConsumerDidScroll(model)
  }

  func onNewSections(sections: [PopupMatchSection], scrollProxy: ScrollViewProxy) {
    // Scroll to the very top of the list.
    scrollProxy.scrollTo(IndexPath(row: 0, section: 0), anchor: UnitPoint(x: 0, y: -.infinity))
  }

  func onNewScrollingOffset(oldScrollingOffset: CGFloat?, newScrollingOffset: CGFloat) {
    guard let initialScrollingOffset = initialScrollingOffset else {
      initialScrollingOffset = newScrollingOffset
      scrollingOffset = newScrollingOffset
      return
    }

    guard let oldScrollingOffset = oldScrollingOffset else { return }

    scrollingOffset = newScrollingOffset
    // Scrolling offset variation is interpreted as drag gesture from user iff:
    // 1. the variation is contained within an acceptable range
    // 2. the new scrolling offset is not too close to the top of the list.
    let scrollingOffsetDelta = abs(oldScrollingOffset - newScrollingOffset)
    let distToInitialScrollingOffset = abs(newScrollingOffset - initialScrollingOffset)
    if Dimensions.acceptableScrollingOffsetDeltaRange.contains(scrollingOffsetDelta)
      && distToInitialScrollingOffset > Dimensions.initialScrollingOffsetDifferenceThreshold
    {
      onScroll()
    }
  }
}

struct PopupView_Previews: PreviewProvider {
  static var previews: some View {
    PopupView(
      model: PopupModel(
        matches: [PopupMatch.previews], headers: ["Suggestions"], delegate: nil)
    )
    .environment(\.sizeCategory, .accessibilityExtraLarge)

    PopupView(
      model: PopupModel(
        matches: [PopupMatch.previews], headers: ["Suggestions"], delegate: nil))
  }
}
