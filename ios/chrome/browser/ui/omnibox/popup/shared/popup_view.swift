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

    enum VariationOne {
      static let pedalSectionSeparatorPadding = EdgeInsets(
        top: 8.5, leading: 0, bottom: 4, trailing: 0)
      static let visibleTopContentInset: CGFloat = 4
      // This allows hiding floating section headers at the top of the list in variation one.
      static let hiddenTopContentInset: CGFloat = -50
    }

    enum VariationTwo {
      // Default height if no other header or footer. This spaces the sections
      // out properly in variation two.
      static let headerFooterHeight: CGFloat = 16

      // Default list row insets. These are removed to inset the popup to the
      // width of the omnibox.
      static let defaultInset: CGFloat = 16
    }
  }

  /// Applies custom list style according to UI variation.
  struct ListStyleModifier: ViewModifier {
    @Environment(\.popupUIVariation) var popupUIVariation: PopupUIVariation
    func body(content: Content) -> some View {
      switch popupUIVariation {
      case .one:
        content.listStyle(.plain)
      case .two:
        content.listStyle(.insetGrouped)
      }
    }
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

  @Environment(\.colorScheme) var colorScheme: ColorScheme
  @Environment(\.popupUIVariation) var popupUIVariation: PopupUIVariation
  @Environment(\.horizontalSizeClass) var sizeClass

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

  func listContent(geometry: GeometryProxy) -> some View {
    ForEach(Array(zip(model.sections.indices, model.sections)), id: \.0) {
      sectionIndex, section in

      let sectionContents =
        ForEach(Array(zip(section.matches.indices, section.matches)), id: \.0) {
          matchIndex, match in
          let indexPath = IndexPath(row: matchIndex, section: sectionIndex)
          let highlighted = indexPath == model.highlightedMatchIndexPath

          PopupMatchRowView(
            match: match,
            isHighlighted: highlighted,
            selectionHandler: {
              model.delegate?.autocompleteResultConsumer(
                model, didSelectRow: UInt(matchIndex), inSection: UInt(sectionIndex))
            },
            trailingButtonHandler: {
              model.delegate?.autocompleteResultConsumer(
                model, didTapTrailingButtonForRow: UInt(matchIndex),
                inSection: UInt(sectionIndex))
            },
            shouldDisplayCustomSeparator: (!highlighted && matchIndex < section.matches.count - 1)
          )
          .id(indexPath)
          .deleteDisabled(!match.supportsDeletion)
          .listRowInsets(Dimensions.matchListRowInsets)
          .listRowBackground(Color.clear)
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

      let footerForVariationTwo = Spacer()
        // Use `leastNonzeroMagnitude` to remove the footer. Otherwise,
        // it uses a default height.
        .frame(height: CGFloat.leastNonzeroMagnitude)
        .listRowInsets(EdgeInsets())

      Section(
        header: header(for: section, at: sectionIndex, geometry: geometry),
        footer: popupUIVariation == .one ? nil : footerForVariationTwo
      ) {
        sectionContents
      }
    }
  }

  // Memory of scrolling offset, so as to be able to tell the difference
  // between user drag gesture and automatic scroll-to-top on new matches.
  @State private var initialScrollingOffset: CGFloat? = nil
  @State private var scrollingOffset: CGFloat? = nil

  @ViewBuilder
  var listView: some View {
    let commonListModifier = AccessibilityIdentifierModifier(
      identifier: kOmniboxPopupTableViewAccessibilityIdentifier
    )
    .concat(ScrollOnChangeModifier(value: $model.sections, action: onNewSections))
    .concat(ListStyleModifier())
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
              listContent(geometry: geometry)
                .anchorPreference(key: MinYPreferenceKey.self, value: .bounds) { geometry[$0].minY }
            },
            emptySpace: {
              PopupEmptySpaceView()
            }
          )
          .frame(width: geometry.size.width, height: geometry.size.height)
          .onPreferenceChange(SelfSizingListHeightPreferenceKey.self) { height in
            selfSizingListHeight = height
          }
        }
      } else {
        List {
          listContent(geometry: geometry)
            .anchorPreference(key: MinYPreferenceKey.self, value: .bounds) { geometry[$0].minY }
        }
        // This fixes list section header internal representation from overlapping safe areas.
        .padding([.leading, .trailing], 0.2)
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
  func header(for section: PopupMatchSection, at index: Int, geometry: GeometryProxy) -> some View {
    if section.header.isEmpty {
      if popupUIVariation == .one {
        if index == 0 {
          // Additional space between omnibox and top section.
          let firstSectionHeader =
            Color.cr_primaryBackground.frame(
              width: geometry.size.width, height: -Dimensions.VariationOne.hiddenTopContentInset)
          if #available(iOS 15.0, *) {
            // Additional padding is added on iOS 15, which needs to be cancelled here.
            firstSectionHeader.padding([.top, .bottom], -6)
          } else {
            firstSectionHeader
          }
        } else if index == 1 {
          // Spacing and separator below the top (pedal) section is inserted as
          // a header in the second section.
          let separatorColor = (colorScheme == .dark) ? Color.cr_grey700 : Color.cr_grey200
          let pedalSectionSeparator =
            separatorColor
            .frame(width: geometry.size.width, height: 0.5)
            .padding(Dimensions.VariationOne.pedalSectionSeparatorPadding)
            .background(Color.cr_primaryBackground)

          if #available(iOS 15.0, *) {
            pedalSectionSeparator.padding([.top, .bottom], -6)
          } else {
            pedalSectionSeparator
          }
        } else {
          EmptyView()
        }
      } else {
        Spacer()
          .frame(height: Dimensions.VariationTwo.headerFooterHeight)
          .listRowInsets(EdgeInsets())
      }
    } else {
      Text(section.header)
    }
  }

  /// Returns a `ViewModifier` to correctly space the sides of the list based
  /// on the current omnibox spacing
  var omniboxPaddingModifier: some ViewModifier {
    let leadingSpace: CGFloat
    let trailingSpace: CGFloat
    if sizeClass == .compact {
      leadingSpace = 0
      trailingSpace = 0
    } else {
      leadingSpace = model.omniboxLeadingSpace
      trailingSpace = model.omniboxTrailingSpace
    }
    let inset: CGFloat =
      (popupUIVariation == .one || sizeClass == .compact)
      ? 0 : -Dimensions.VariationTwo.defaultInset
    return PaddingModifier([.leading], leadingSpace + inset).concat(
      PaddingModifier([.trailing], trailingSpace + inset))
  }

  var listBackground: some View {
    let backgroundColor: Color
    // iOS 14 + SwiftUI has a bug with dark mode colors in high contrast mode.
    // If no dark mode + high contrast color is provided, they are supposed to
    // default to the dark mode color. Instead, SwiftUI defaults to the light
    // mode color. This bug is fixed in iOS 15, but until then, a workaround
    // color with the dark mode + high contrast color specified is used.
    if #available(iOS 15, *) {
      backgroundColor =
        (popupUIVariation == .one) ? Color.cr_primaryBackground : Color.cr_groupedPrimaryBackground
    } else {
      backgroundColor =
        (popupUIVariation == .one)
        ? Color("primary_background_color_swiftui_ios14")
        : Color("grouped_primary_background_color_swiftui_ios14")
    }
    return backgroundColor.edgesIgnoringSafeArea(.all)
  }

  func onAppear() {
    if let appearanceContainerType = self.appearanceContainerType {
      let listAppearance = UITableView.appearance(whenContainedInInstancesOf: [
        appearanceContainerType
      ])

      listAppearance.backgroundColor = .clear
      // Custom separators are provided, so the system ones are made invisible.
      listAppearance.separatorColor = .clear
      listAppearance.separatorStyle = .none
      listAppearance.separatorInset = .zero

      if #available(iOS 15.0, *) {
        listAppearance.sectionHeaderTopPadding = 0
      } else {
        listAppearance.sectionFooterHeight = 0
      }

      if popupUIVariation == .one {
        listAppearance.contentInset.top = Dimensions.VariationOne.hiddenTopContentInset
        listAppearance.contentInset.top += Dimensions.VariationOne.visibleTopContentInset
      }

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
        matches: [PopupMatch.previews], headers: ["Suggestions"], delegate: nil)
    )
    .environment(\.popupUIVariation, .one)
  }
}
