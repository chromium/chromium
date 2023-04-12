// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A view displaying a list of destinations.
@available(iOS 15, *)
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

    /// When the dynamic text size is large, the width of each item is the
    /// screen width minus a fixed space.
    static let largeTextSizeSpace: CGFloat = 120

    /// Space above the list pushing them down from the grabber.
    static let topMargin: CGFloat = 20

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

  /// The current dynamic type size.
  @Environment(\.sizeCategory) var sizeCategory

  /// The current environment layout direction.
  @Environment(\.layoutDirection) var layoutDirection: LayoutDirection

  /// The destinations for this view.
  var destinations: [OverflowMenuDestination]

  weak var metricsHandler: PopupMenuMetricsHandler?

  @ObservedObject var uiConfiguration: OverflowMenuUIConfiguration

  /// Tracks the list's current offset, to see when it scrolls. When the offset
  /// is `nil`, scroll tracking is not set up yet. This is necessary because
  /// in RTL languages, the scroll view has to manually scroll to the right edge
  /// of the list first.
  @State var listOffset: CGFloat? = nil

  var body: some View {
    VStack {
      Spacer(minLength: Constants.topMargin)
      GeometryReader { geometry in
        scrollView(in: geometry)
          .coordinateSpace(name: Constants.coordinateSpaceName)
          .accessibilityIdentifier(kPopupMenuToolsMenuTableViewId)
      }
    }
    .background(
      Color("destination_highlight_color").opacity(uiConfiguration.highlightDestinationsRow ? 1 : 0)
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
  private func scrollView(in geometry: GeometryProxy) -> some View {
    ScrollViewReader { proxy in
      ScrollView(.horizontal, showsIndicators: false) {
        let spacing = OverflowMenuDestinationList.destinationSpacing(
          forScreenWidth: geometry.size.width)
        let layoutParameters = OverflowMenuDestinationList.layoutParameters(
          forScreenWidth: geometry.size.width, forSizeCategory: sizeCategory)
        let alignment: VerticalAlignment = sizeCategory >= .accessibilityMedium ? .center : .top

        ZStack {
          HStack(alignment: alignment, spacing: 0) {
            // Make sure the space to the first icon is constant, so add extra
            // spacing before the first item.
            Spacer().frame(width: Constants.iconInitialSpace - spacing.iconSpacing)
            ForEach(destinations) { destination in
              OverflowMenuDestinationView(
                destination: destination, layoutParameters: layoutParameters,
                metricsHandler: metricsHandler
              ).id(destination.destination)
            }
          }

          GeometryReader { innerGeometry in
            let frame = innerGeometry.frame(in: .named(Constants.coordinateSpaceName))
            let parentWidth = geometry.size.width

            // When the view is RTL, the offset should be calculated from the
            // right edge.
            let offset = layoutDirection == .leftToRight ? frame.minX : parentWidth - frame.maxX

            Color.clear
              .preference(key: ScrollViewLeadingOffset.self, value: offset)
          }
        }
      }
      .onAppear {
        if layoutDirection == .rightToLeft {
          proxy.scrollTo(destinations.first?.destination)
        }
        uiConfiguration.destinationListScreenFrame = geometry.frame(in: .global)
      }
    }
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
      ? .horizontal(itemWidth: width - Constants.largeTextSizeSpace)
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
    let destinationWidth = OverflowMenuDestinationButton.destinationWidth(
      forLayoutParameters: layoutParameters)

    return (width / destinationWidth).rounded(.up)
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
