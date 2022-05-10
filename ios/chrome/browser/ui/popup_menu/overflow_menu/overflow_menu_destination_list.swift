// Copyright 2021 The Chromium Authors. All rights reserved.
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
  }

  /// The current dynamic type size.
  @Environment(\.sizeCategory) var sizeCategory

  /// The destinations for this view.
  var destinations: [OverflowMenuDestination]

  weak var metricsHandler: PopupMenuMetricsHandler?

  var body: some View {
    GeometryReader { geometry in
      ScrollView(.horizontal, showsIndicators: false) {
        let spacing = destinationSpacing(forScreenWidth: geometry.size.width)
        let layoutParameters: OverflowMenuDestinationView.LayoutParameters =
          sizeCategory >= .accessibilityMedium
          ? .horizontal(itemWidth: geometry.size.width - Constants.largeTextSizeSpace)
          : .vertical(
            iconSpacing: spacing.iconSpacing,
            iconPadding: spacing.iconPadding)
        let alignment: VerticalAlignment = sizeCategory >= .accessibilityMedium ? .center : .top

        VStack {
          Spacer(minLength: Constants.topMargin)
          LazyHStack(alignment: alignment, spacing: 0) {
            ForEach(destinations) { destination in
              OverflowMenuDestinationView(
                destination: destination, layoutParameters: layoutParameters,
                metricsHandler: metricsHandler)
            }
          }
        }
        // Make sure the space to the first icon is constant, so add extra
        // spacing before the first item.
        .padding([.leading], Constants.iconInitialSpace - spacing.iconSpacing)
      }
      .accessibilityIdentifier(kPopupMenuToolsMenuTableViewId)
    }
  }

  /// Finds the lower and upper breakpoint above and below `width`.
  ///
  /// Returns `nil` for either end if `width` is above or below the largest or
  /// smallest breakpoint.
  private func findBreakpoints(forScreenWidth width: CGFloat) -> (CGFloat?, CGFloat?) {
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
  private func destinationSpacing(forScreenWidth width: CGFloat) -> (
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

  /// Maps the given `number` from its relative position in `inRange` to its
  /// relative position in `outRange`.
  private func mapNumber<F: FloatingPoint>(
    _ number: F, from inRange: ClosedRange<F>, to outRange: ClosedRange<F>
  ) -> F {
    let scalingFactor =
      (outRange.upperBound - outRange.lowerBound)
      / (inRange.upperBound - inRange.lowerBound)
    return (number - inRange.lowerBound) * scalingFactor + outRange.lowerBound
  }
}
