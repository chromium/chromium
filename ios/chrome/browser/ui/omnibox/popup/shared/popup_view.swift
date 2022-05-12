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

/// Utility which provides a way to treat the `accessibilityIdentifier` view modifier as a value.
struct AccessibilityIdentifierModifier: ViewModifier {
  let identifier: String
  func body(content: Content) -> some View {
    content.accessibilityIdentifier(identifier)
  }
}

/// Returns the closest pixel-aligned value higher than `value`, taking the scale
/// factor into account. At a scale of 1, equivalent to ceil().
func alignValueToUpperPixel(_ value: CGFloat) -> CGFloat {
  let scale = UIScreen.main.scale
  return ceil(value * scale) / scale
}

struct PopupView: View {
  enum Dimensions {
    static let matchListRowInsets = EdgeInsets(.zero)

    enum VariationOne {
      static let pedalSectionSeparatorPadding = EdgeInsets(
        top: 8.5, leading: 0, bottom: 4, trailing: 0)
      static let visibleTopContentInset: CGFloat = 4
      // This allows hiding floating section headers at the top of the list in variation one.
      static let hiddenTopContentInset: CGFloat = -50

      static let selfSizingListBottomMargin: CGFloat = 8
    }

    enum VariationTwo {
      // Default height if no other header or footer. This spaces the sections
      // out properly in variation two.
      static let headerFooterHeight: CGFloat = 16

      // Default list row insets. These are removed to inset the popup to the
      // width of the omnibox.
      static let defaultInset: CGFloat = 16

      static let selfSizingListBottomMargin: CGFloat = 16
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

  /// Calls `onScroll` when the user performs a drag gesture over the content of the list.
  struct ListScrollDetectionModifier: ViewModifier {
    let onScroll: () -> Void
    func body(content: Content) -> some View {
      content
        // For some reason, without this, user interaction is not forwarded to the list.
        // Setting the count to more than one ensures a buggy SwiftUI will not
        // ignore actual tap gestures in subviews.
        .onTapGesture(count: 2) {}
        // Long press gestures are dismissed and `onPressingChanged` called with
        // `pressing` equal to `false` when the user performs a drag gesture
        // over the content, hence why this works. `DragGesture` cannot be used
        // here, even with a `simultaneousGesture` modifier, because it prevents
        // swipe-to-delete from correctly triggering within the list.
        .onLongPressGesture(
          perform: {},
          onPressingChanged: { pressing in
            if !pressing {
              onScroll()
            }
          })
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
  @ObservedObject var uiConfiguration: PopupUIConfiguration
  private let shouldSelfSize: Bool
  private let appearanceContainerType: UIAppearanceContainer.Type?

  @Environment(\.colorScheme) var colorScheme: ColorScheme
  @Environment(\.popupUIVariation) var popupUIVariation: PopupUIVariation
  @Environment(\.horizontalSizeClass) var sizeClass

  /// The current height of the self-sizing list.
  @State private var selfSizingListHeight: CGFloat? = nil

  init(
    model: PopupModel, uiConfiguration: PopupUIConfiguration, shouldSelfSize: Bool = false,
    appearanceContainerType: UIAppearanceContainer.Type? = nil
  ) {
    self.model = model
    self.uiConfiguration = uiConfiguration
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
          // UI Variation 1 always has separators. Variation 2 doesn't have
          // one on the last row in a section.
          // If there is only one row or less in a section, no row separators.
          let shouldDisplayCustomSeparator =
            !highlighted && (section.matches.count > 1)
            && (matchIndex < section.matches.count - 1)

          PopupMatchRowView(
            match: match,
            isHighlighted: highlighted,
            toolbarConfiguration: uiConfiguration.toolbarConfiguration,
            selectionHandler: {
              model.delegate?.autocompleteResultConsumer(
                model, didSelectRow: UInt(matchIndex), inSection: UInt(sectionIndex))
            },
            trailingButtonHandler: {
              model.delegate?.autocompleteResultConsumer(
                model, didTapTrailingButtonForRow: UInt(matchIndex),
                inSection: UInt(sectionIndex))
            },
            uiConfiguration: uiConfiguration,
            shouldDisplayCustomSeparator: shouldDisplayCustomSeparator
          )
          .id(indexPath)
          .deleteDisabled(!match.supportsDeletion)
          .listRowInsets(Dimensions.matchListRowInsets)
          .listRowBackground(Color.clear)
          .accessibilityElement(children: .combine)
          .accessibilityIdentifier(
            OmniboxPopupAccessibilityIdentifierHelper.accessibilityIdentifierForRow(at: indexPath)
          )
          .environment(\.layoutDirection, layoutDirection)
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

  @ViewBuilder
  var bottomSeparator: some View {
    Color.toolbarShadow.frame(height: alignValueToUpperPixel(kToolbarSeparatorHeight))
  }

  var selfSizingListBottomMargin: CGFloat {
    switch popupUIVariation {
    case .one:
      return Dimensions.VariationOne.selfSizingListBottomMargin
    case .two:
      return Dimensions.VariationTwo.selfSizingListBottomMargin
    }
  }

  @ViewBuilder
  var listView: some View {
    let commonListModifier = AccessibilityIdentifierModifier(
      identifier: kOmniboxPopupTableViewAccessibilityIdentifier
    )
    .concat(ListScrollDetectionModifier(onScroll: onScroll))
    .concat(ScrollOnChangeModifier(value: $model.sections, action: onNewSections))
    .concat(ListStyleModifier())
    .concat(EnvironmentValueModifier(\.defaultMinListHeaderHeight, 0))
    .concat(omniboxPaddingModifier)

    GeometryReader { geometry in
      ZStack(alignment: .top) {
        listBackground.frame(height: selfSizingListHeight)
        if shouldSelfSize {
          ZStack(alignment: .top) {
            SelfSizingList(
              bottomMargin: selfSizingListBottomMargin,
              listModifier: commonListModifier,
              content: {
                listContent(geometry: geometry)
              },
              emptySpace: {
                PopupEmptySpaceView()
              }
            )
            .frame(width: geometry.size.width, height: geometry.size.height)
            .onPreferenceChange(SelfSizingListHeightPreferenceKey.self) { height in
              selfSizingListHeight = height
            }
            bottomSeparator.offset(x: 0, y: selfSizingListHeight ?? 0)
          }
        } else {
          List {
            listContent(geometry: geometry)
          }
          // This fixes list section header internal representation from overlapping safe areas.
          .padding([.leading, .trailing], 0.2)
          .modifier(commonListModifier)
          .ignoresSafeArea(.keyboard)
          .ignoresSafeArea(.container, edges: [.leading, .trailing])
          .frame(width: geometry.size.width, height: geometry.size.height)
        }
      }
    }
  }

  var body: some View {
    listView
      .onAppear(perform: onAppear)
      .environment(\.layoutDirection, .leftToRight)
  }

  @ViewBuilder
  func header(for section: PopupMatchSection, at index: Int, geometry: GeometryProxy) -> some View {
    if section.header.isEmpty {
      if popupUIVariation == .one {
        if index == 0 {
          // Additional space between omnibox and top section.
          let firstSectionHeader = Color(uiConfiguration.toolbarConfiguration.backgroundColor)
            .frame(
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
          let separatorColor = Color.separator
          let pedalSectionSeparator =
            separatorColor
            .frame(width: geometry.size.width, height: 0.5)
            .padding(Dimensions.VariationOne.pedalSectionSeparatorPadding)
            .background(Color(uiConfiguration.toolbarConfiguration.backgroundColor))

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

  @Environment(\.layoutDirection) var layoutDirection: LayoutDirection

  /// Returns a `ViewModifier` to correctly space the sides of the list based
  /// on the current omnibox spacing
  var omniboxPaddingModifier: some ViewModifier {
    let leadingSpace: CGFloat
    let trailingSpace: CGFloat
    let leadingHorizontalMargin: CGFloat
    let trailingHorizontalMargin: CGFloat
    switch popupUIVariation {
    case .one:
      leadingSpace = 0
      trailingSpace = 0
      leadingHorizontalMargin = 0
      trailingHorizontalMargin = 0
    case .two:
      leadingSpace = uiConfiguration.omniboxLeadingSpace - Dimensions.VariationTwo.defaultInset
      trailingSpace =
        (shouldSelfSize && sizeClass != .compact
          ? uiConfiguration.omniboxTrailingSpace : uiConfiguration.safeAreaTrailingSpace)
        - Dimensions.VariationTwo.defaultInset
      leadingHorizontalMargin = 0
      trailingHorizontalMargin = sizeClass == .compact ? kContractedLocationBarHorizontalMargin : 0
    }
    return PaddingModifier([.leading], leadingSpace + leadingHorizontalMargin).concat(
      PaddingModifier([.trailing], trailingSpace + trailingHorizontalMargin))
  }

  var listBackground: some View {
    let backgroundColor: Color
    // iOS 14 + SwiftUI has a bug with dark mode colors in high contrast mode.
    // If no dark mode + high contrast color is provided, they are supposed to
    // default to the dark mode color. Instead, SwiftUI defaults to the light
    // mode color. This bug is fixed in iOS 15, but until then, a workaround
    // color with the dark mode + high contrast color specified is used.
    if #available(iOS 15, *) {
      switch popupUIVariation {
      case .one:
        backgroundColor = Color(uiConfiguration.toolbarConfiguration.backgroundColor)
      case .two:
        backgroundColor = .groupedPrimaryBackground
      }
    } else {
      switch popupUIVariation {
      case .one:
        backgroundColor = Color("background_color_swiftui_ios14")
      case .two:
        backgroundColor = Color("grouped_primary_background_color_swiftui_ios14")
      }
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
    if !sections.isEmpty || !sections.first!.matches.isEmpty {
      scrollProxy.scrollTo(IndexPath(row: 0, section: 0), anchor: UnitPoint(x: 0, y: -.infinity))
    }
  }
}

struct PopupView_Previews: PreviewProvider {
  static var previews: some View {
    let sample =
      PopupView(
        model: PopupModel(
          matches: [PopupMatch.previews], headers: ["Suggestions"], delegate: nil),
        uiConfiguration: PopupUIConfiguration(
          toolbarConfiguration: ToolbarConfiguration(style: .NORMAL))
      )

    sample.environment(\.popupUIVariation, .one)
    sample.environment(\.popupUIVariation, .two)
    sample.environment(\.locale, .init(identifier: "ar"))

    let darkSample = sample.environment(\.colorScheme, .dark)

    darkSample.environment(\.popupUIVariation, .one)
    darkSample.environment(\.popupUIVariation, .two)

    let sampleWithExtraLargeFont =
      sample.environment(\.sizeCategory, .accessibilityExtraLarge)

    sampleWithExtraLargeFont.environment(\.popupUIVariation, .one)
    sampleWithExtraLargeFont.environment(\.popupUIVariation, .two)
  }
}
