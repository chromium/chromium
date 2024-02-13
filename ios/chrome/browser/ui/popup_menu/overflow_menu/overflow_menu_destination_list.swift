// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Combine
import SwiftUI

/// Compatibility modifier to allow easy usage of `.scrollClipDisabled`
/// introduced in iOS 17.
struct ScrollClipDisabledCompat: ViewModifier {
  var disabled: Bool
  func body(content: Content) -> some View {
    #if swift(>=5.9)
      if #available(iOS 17, *) {
        return content.scrollClipDisabled(disabled)
      }
    #endif
    return content
  }
}

extension View {
  func scrollClipDisabledCompat(_ disabled: Bool = true) -> some View {
    modifier(ScrollClipDisabledCompat(disabled: disabled))
  }
}

/// A view displaying a list of destinations.
struct OverflowMenuDestinationList: View {
  enum Constants {
    /// Padding breakpoints for each width. The ranges should be inclusive of
    /// the larger number. That is, a width of 320 should fall in the
    /// `(230, 320]` bucket.
    static let widthBreakpoints: [CGFloat] = [
      180, 230, 320, 400, 470, 560, 650,
    ]

    /// Array of the lower end of each breakpoint range.
    static let lowerWidthBreakpoints = [nil] + widthBreakpoints

    /// Array of the higher end of each breakpoint range.
    static let upperWidthBreakpoints = widthBreakpoints + [nil]

    /// Leading space on the first icon.
    static let iconInitialSpace: CGFloat = 16

    /// Range of spacing around icons; varies based on view width.
    static let iconSpacingRange: ClosedRange<CGFloat> = 9...13

    /// Range of icon paddings; varies based on view width.
    static let iconPaddingRange: ClosedRange<CGFloat> = 0...3

    /// The top margin between the destinations and the edge of the list.
    static let defaultTopMargin: CGFloat = 15

    static let defaultBottomMargin: CGFloat = 8

    /// The name for the coordinate space of the scroll view, so children can
    /// find their positioning in the scroll view.
    static let coordinateSpaceName = "destinations"
  }

  /// `PreferenceKey` to track the leading offset of the scroll view.
  struct ScrollViewLeadingOffset: PreferenceKey {
    static var defaultValue: CGFloat = .greatestFiniteMagnitude
    static func reduce(value: inout CGFloat, nextValue: () -> CGFloat) {
      value = min(value, nextValue())
    }
  }

  /// `PreferenceKey` to track the highlighted destination's bounds in its local coordinate space,
  /// can be transformed to other coordinate space by geometry reader.
  struct HighlightedDestinationBounds: PreferenceKey {
    typealias Value = Anchor<CGRect>?
    static var defaultValue: Value = nil
    static func reduce(value: inout Value, nextValue: () -> Value) {
      // AnchorPreference might be nil in the middle of layout.
      if let next = nextValue() {
        value = next
      }
    }
  }

  /// The current dynamic type size.
  @Environment(\.sizeCategory) var sizeCategory

  /// The current environment layout direction.
  @Environment(\.layoutDirection) var layoutDirection: LayoutDirection

  @Environment(\.editMode) var editMode

  /// The destinations for this view.
  @Binding var destinations: [OverflowMenuDestination]

  // The allotted width of this view.
  var width: CGFloat

  weak var metricsHandler: PopupMenuMetricsHandler?

  @ObservedObject var uiConfiguration: OverflowMenuUIConfiguration

  // The drag handler to use for drag interactions on this list
  @ObservedObject var dragHandlerContainer: DestinationDragHandlerContainer

  /// The namespace for the animation of this view appearing or disappearing.
  let namespace: Namespace.ID

  /// Tracks the list's current offset, to see when it scrolls. When the offset
  /// is `nil`, scroll tracking is not set up yet. This is necessary because
  /// in RTL languages, the scroll view has to manually scroll to the right edge
  /// of the list first.
  @State var listOffset: CGFloat? = nil

  init(
    destinations: Binding<[OverflowMenuDestination]>,
    width: CGFloat,
    metricsHandler: PopupMenuMetricsHandler? = nil,
    uiConfiguration: OverflowMenuUIConfiguration,
    dragHandler: DestinationDragHandler? = nil,
    namespace: Namespace.ID
  ) {
    self._destinations = destinations
    self.width = width
    self.metricsHandler = metricsHandler
    self.uiConfiguration = uiConfiguration
    dragHandlerContainer = DestinationDragHandlerContainer(dragHandler: dragHandler)
    self.namespace = namespace
  }

  var body: some View {
    scrollView
      .coordinateSpace(name: Constants.coordinateSpaceName)
      .accessibilityIdentifier(kPopupMenuToolsMenuTableViewId)
      .background(
        Color("destination_highlight_color").opacity(
          uiConfiguration.highlightDestinationsRow ? 1 : 0)
      )
      .animation(
        .linear(duration: kMaterialDuration3), value: uiConfiguration.highlightDestinationsRow
      )
      .onPreferenceChange(ScrollViewLeadingOffset.self) { newOffset in
        // Only alert the handler if scroll tracking has started.
        if let listOffset = listOffset,
          newOffset != listOffset
        {
          metricsHandler?.popupMenuScrolledHorizontally()
        }
        // Only update the offset if scroll tracking has started or the newOffset
        // is approximately 0 (this starts scroll tracking). In RTL mode, the
        // offset is not exactly 0, so a strict comparison won't work.
        if listOffset != nil || (listOffset == nil && abs(newOffset) < 1e-9) {
          listOffset = newOffset
        }
      }
  }

  @ViewBuilder
  private var scrollView: some View {
    ScrollViewReader { proxy in
      ScrollView(.horizontal, showsIndicators: false) {
        let spacing = OverflowMenuDestinationList.destinationSpacing(
          forScreenWidth: width)
        let layoutParameters = OverflowMenuDestinationList.layoutParameters(
          forScreenWidth: width, forSizeCategory: sizeCategory)
        let alignment: VerticalAlignment = sizeCategory >= .accessibilityMedium ? .center : .icon
        HStack(alignment: alignment, spacing: 0) {
          // Make sure the space to the first icon is constant, so add extra
          // spacing before the first item.
          Spacer().frame(width: Constants.iconInitialSpace - spacing.iconSpacing)
          ForEach(destinations) { destination in
            let destinationView = OverflowMenuDestinationView(
              destination: destination, layoutParameters: layoutParameters,
              highlighted: uiConfiguration.highlightDestination == destination.destination,
              metricsHandler: metricsHandler
            )
            let destinationBeingDragged =
              dragHandlerContainer.dragHandler?.dragOnDestinations ?? false
              && dragHandlerContainer.dragHandler?.currentDrag?.item == destination
            destinationView
              .id(destination.destination)
              .ifLet(dragHandlerContainer.dragHandler) { view, dragHandler in
                view
                  .opacity(destinationBeingDragged ? 0.01 : 1)
                  .onDrag {
                    dragHandler.startDrag(from: destination)
                    return dragHandler.newItemProvider(forDestination: destination)
                  }
                  .onDrop(
                    of: [.text],
                    delegate: dragHandler.newDropDelegate(
                      forDestination: destination))
              }
              .overlay(alignment: .editButton) {
                if !destinationBeingDragged && editMode?.wrappedValue.isEditing == true
                  && destination.canBeHidden
                {
                  DestinationEditButton(destination: destination)
                    .alignmentGuide(HorizontalAlignment.editButton) {
                      $0[HorizontalAlignment.center]
                    }
                    .alignmentGuide(VerticalAlignment.editButton) { $0[VerticalAlignment.center] }
                }
              }
              .matchedGeometryEffect(
                id: MenuCustomizationAnimationID.from(destination), in: namespace
              )
              .accessibilityElement(children: .combine)
              .accessibilityHint(editButtonAccessibilityHint(for: destination))
          }
        }
        .fixedSize(horizontal: false, vertical: true)
        .padding([.top], Constants.defaultTopMargin)
        .padding([.bottom], Constants.defaultBottomMargin)
        .overlay {
          GeometryReader { innerGeometry in
            let frame = innerGeometry.frame(in: .named(Constants.coordinateSpaceName))

            // When the view is RTL, the offset should be calculated from the
            // right edge.
            let offset = layoutDirection == .leftToRight ? frame.minX : width - frame.maxX

            Color.clear
              .preference(key: ScrollViewLeadingOffset.self, value: offset)
          }
        }
      }
      .scrollClipDisabledCompat()
      .background {
        GeometryReader { geometry in
          Color.clear.onAppear {
            uiConfiguration.destinationListScreenFrame = geometry.frame(in: .global)
          }
        }
      }
      .onAppear {
        if destinations.map(\.destination).contains(uiConfiguration.highlightDestination) {
          proxy.scrollTo(uiConfiguration.highlightDestination)
        } else if layoutDirection == .rightToLeft {
          proxy.scrollTo(destinations.first?.destination)
        }
      }
    }
  }

  private func editButtonAccessibilityHint(for destination: OverflowMenuDestination) -> String {
    guard editMode?.wrappedValue.isEditing == true && destination.canBeHidden else {
      return ""
    }
    return destination.shown
      ? L10nUtils.stringWithFixup(
        messageId: IDS_IOS_OVERFLOW_MENU_HIDE_ITEM_ACCESSIBILITY_HINT)
      : L10nUtils.stringWithFixup(messageId: IDS_IOS_OVERFLOW_MENU_SHOW_ITEM_ACCESSIBILITY_HINT)
  }

  /// Finds the lower and upper breakpoint above and below `width`.
  ///
  /// Returns `nil` for either end if `width` is above or below the largest or
  /// smallest breakpoint.
  private static func findBreakpoints(forScreenWidth width: CGFloat) -> (CGFloat?, CGFloat?) {
    // Add extra sentinel values to either end of the breakpoint array.
    let x = zip(
      Constants.lowerWidthBreakpoints, Constants.upperWidthBreakpoints
    )
    // There should only be one item where the provided width is both greater
    // than the lower end and less than the upper end.
    .filter {
      (low, high) in
      // Check if width is above the low value, or default to true if low is
      // nil.
      let aboveLow = low.map { value in width > value } ?? true
      let belowHigh = high.map { value in width <= value } ?? true
      return aboveLow && belowHigh
    }.first
    return x ?? (nil, nil)
  }

  /// Calculates the icon spacing and padding for the given `width`.
  private static func destinationSpacing(forScreenWidth width: CGFloat) -> (
    iconSpacing: CGFloat, iconPadding: CGFloat
  ) {
    let (lowerBreakpoint, upperBreakpoint) = findBreakpoints(
      forScreenWidth: width)

    // If there's no lower breakpoint, `width` is lower than the lowest, so
    // default to the lower bound of the ranges.
    guard let lowerBreakpoint = lowerBreakpoint else {
      return (
        iconSpacing: Constants.iconSpacingRange.lowerBound,
        iconPadding: Constants.iconPaddingRange.lowerBound
      )
    }

    // If there's no upper breakpoint, `width` is higher than the highest, so
    // default to the higher bound of the ranges.
    guard let upperBreakpoint = upperBreakpoint else {
      return (
        iconSpacing: Constants.iconSpacingRange.upperBound,
        iconPadding: Constants.iconPaddingRange.upperBound
      )
    }

    let breakpointRange = lowerBreakpoint...upperBreakpoint

    let iconSpacing = mapNumber(
      width, from: breakpointRange, to: Constants.iconSpacingRange)
    let iconPadding = mapNumber(
      width, from: breakpointRange, to: Constants.iconPaddingRange)
    return (iconSpacing: iconSpacing, iconPadding: iconPadding)
  }

  private static func layoutParameters(
    forScreenWidth width: CGFloat, forSizeCategory sizeCategory: ContentSizeCategory
  ) -> OverflowMenuDestinationView.LayoutParameters {
    let spacing = OverflowMenuDestinationList.destinationSpacing(forScreenWidth: width)

    return sizeCategory >= .accessibilityMedium
      ? .horizontal
      : .vertical(
        iconSpacing: spacing.iconSpacing,
        iconPadding: spacing.iconPadding)
  }

  public static func numDestinationsVisibleWithoutHorizontalScrolling(
    forScreenWidth width: CGFloat, forSizeCategory sizeCategory: ContentSizeCategory
  )
    -> CGFloat
  {
    let layoutParameters = OverflowMenuDestinationList.layoutParameters(
      forScreenWidth: width, forSizeCategory: sizeCategory)

    switch layoutParameters {
    case .vertical(let iconSpacing, let iconPadding):
      let destinationWidth = OverflowMenuDestinationView.verticalLayoutDestinationWidth(
        iconSpacing: iconSpacing, iconPadding: iconPadding)

      return (width / destinationWidth).rounded(.up)
    case .horizontal:
      // In horizontal layout, the width of an individual item depends on the
      // text length. However, it'll always be pretty long, so 2 is a good
      // estimate.
      return 2
    }

  }

  /// Maps the given `number` from its relative position in `inRange` to its
  /// relative position in `outRange`.
  private static func mapNumber<F: FloatingPoint>(
    _ number: F, from inRange: ClosedRange<F>, to outRange: ClosedRange<F>
  ) -> F {
    let scalingFactor =
      (outRange.upperBound - outRange.lowerBound)
      / (inRange.upperBound - inRange.lowerBound)
    return (number - inRange.lowerBound) * scalingFactor + outRange.lowerBound
  }
}

extension VerticalAlignment {
  /// A new custom alignment to align the DestinationViews by their icon
  /// position.
  static let icon = VerticalAlignment(Icon.self)

  /// A new custom alignment to allow aligning the edit buttons at specific
  /// locations.
  static let editButton = VerticalAlignment(EditButton.self)

  private enum Icon: AlignmentID {
    static func defaultValue(in d: ViewDimensions) -> CGFloat {
      return d[.bottom]
    }
  }

  private enum EditButton: AlignmentID {
    static func defaultValue(in d: ViewDimensions) -> CGFloat {
      return d[.top]
    }
  }
}

extension HorizontalAlignment {
  /// A new custom alignment to allow aligning the edit buttons at specific
  /// locations.
  static let editButton = HorizontalAlignment(EditButton.self)

  private enum EditButton: AlignmentID {
    static func defaultValue(in d: ViewDimensions) -> CGFloat {
      return d[.leading]
    }
  }
}

extension Alignment {
  /// A new custom alignment to allow aligning the edit buttons at specific
  /// locations.
  static let editButton = Alignment(horizontal: .editButton, vertical: .editButton)
}

/// Before iOS 17, it was not possible to directly observe optional objects
/// e.g.
/// ```
/// @ObservedObject var myOptional: Foo?
/// ```
/// `DestinationDragHandler` is often optional, so this simple class wraps it
/// in a container that just re-publishes any changes to the underlying drag
/// handler.
/// The `Observable` macro in iOS 17 looks to also solve this issue, and
/// this should be migrateable once iOS 17 is the minimum version supported.
class DestinationDragHandlerContainer: ObservableObject {
  // The underlying drag handler.
  let dragHandler: DestinationDragHandler?

  var cancellable: AnyCancellable?

  init(dragHandler: DestinationDragHandler?) {
    self.dragHandler = dragHandler

    cancellable = dragHandler?.objectWillChange.sink { [weak self] in
      self?.objectWillChange.send()
    }
  }
}
