// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Charts
import Foundation
import SwiftUI

/// `PreferenceKey` used to retrieve the width of a view during the layout process.
struct TooltipViewWidthKey: PreferenceKey {
  static var defaultValue: CGFloat = 0
  static func reduce(value: inout CGFloat, nextValue: () -> CGFloat) {
    value = nextValue()
  }
}

/// Represents a view displaying a tooltip with the date and corresponding price.
struct TooltipView: View {
  /// Properties for the content and position of the tooltip.
  var currency: String
  var price: Double
  var date: Date
  var xPosition: CGFloat
  var chartWidth: CGFloat

  /// Corner radius.
  static let cornerRadius = 44.0

  /// Vertical padding.
  static let verticalPadding = 8.0

  /// Horizontal padding.
  static let horizontalPadding = 2.0

  /// Size of the text.
  static let textSize = 11.0

  /// Color for the tool tip background.
  static let tooltipBackgroundColor = "tooltip_background_color"

  /// Color for the tool tip text.
  static let tooltipTextColor = "tooltip_text_color"

  /// Tooltip width value.
  @State private var tooltipWidth: CGFloat = 0

  /// layoutDirection environment value.
  @Environment(\.layoutDirection) var layoutDirection

  var body: some View {
    var tooltipText: String {
      let priceFormatted =
        price.formatted(.currency(code: currency).precision(.fractionLength(0)))
      let dateFormatted = date.formatted(date: .abbreviated, time: .omitted)
      return layoutDirection == .leftToRight
        ? "\(priceFormatted) \(dateFormatted)" : "\(dateFormatted) \(priceFormatted)"
    }

    Text(tooltipText)
      .font(.system(size: Self.textSize))
      .foregroundColor(Color(Self.tooltipTextColor))
      .padding([.leading, .trailing], Self.verticalPadding)
      .padding([.bottom, .top], Self.horizontalPadding)
      .background(
        GeometryReader { geo in
          RoundedRectangle(cornerRadius: Self.cornerRadius)
            .fill(Color(Self.tooltipBackgroundColor))
            .preference(key: TooltipViewWidthKey.self, value: geo.size.width)
        }
      )
      .onPreferenceChange(TooltipViewWidthKey.self) { newWidth in
        tooltipWidth = newWidth
      }
      .position(x: xPosition, y: 0.0)
      .offset(
        x: {
          /// Adjusts the horizontal position of the tooltip to ensure it stays
          /// within the chart's bounds and doesn't overflow out of bounds of the chart.
          if xPosition < tooltipWidth / 2 {
            /// If the tooltip is too far to the left, shift it right
            return max(0, abs(xPosition - tooltipWidth / 2))
          }
          if xPosition > (chartWidth - (tooltipWidth / 2)) {
            /// If the tooltip is too far to the right, shift it left
            return min(0, chartWidth - (tooltipWidth / 2) - xPosition)
          }

          return 0.0
        }(), y: 0.0)
  }
}

/// A view modifier that conditionally applies different gestures based on the iOS version for the graph.
struct GraphGesture: ViewModifier {
  let geometry: GeometryProxy
  let proxy: ChartProxy
  let updateSelectionData: (CGPoint, GeometryProxy, ChartProxy) -> Void
  let updateTooltipPosition: (GeometryProxy, ChartProxy) -> Void
  let recordGraphInteraction: () -> Void

  func body(content: Content) -> some View {
    // The minimum distance amount here allows scrolling to take precedence over
    // dragging when the user is scrolling vertically.
    // There are 3 gestures interacting here: dragging on the graph, scrolling
    // the panel, and dragging to expand the panel. The solution uses 2
    // different methods to handle the graph drag's interaction with each of
    // the other two gestures. First, vs scrolling the panel, using a
    // DragGesture with a minimum distance allows the scrolling to supercede
    // the drag if the user moves their finger vertically, but still allows the
    // drag to go off if the user moves their finger horizontally. The scroll
    // view looks like it requires the user to move their finger some short
    // distance vertically before scrolling begins. So if the user moves
    // vertically, the scroll gesture activates before the minimum distance is
    // hit. But if the user moves horiztonally, the graph's minimum distance
    // is hit first, activating that one.
    //
    // For the interaction with the gesture to expand the sheet,
    // UIGestureRecognizerDelegate methods are used elsewhere to add a
    // hierarchical relationship. This makes sure that the expansion gesture
    // doesn't activate until after the graph drag gesture fails. It doesn't
    // matter whether the graph gesture actually fails or not because the sheet
    // expand gesture should never activate when dragging on the graph.
    let gesture = DragGesture(minimumDistance: 15, coordinateSpace: .local)
      .onChanged { value in
        updateSelectionData(value.location, geometry, proxy)
        updateTooltipPosition(geometry, proxy)
      }
      .onEnded { _ in
        recordGraphInteraction()
      }
    #if swift(>=6.0)
      if #available(iOS 18, *) {
        content.gesture(gesture, name: kPanelContentGestureRecognizerName)
      } else {
        content.gesture(gesture)
      }
    #else
      content.gesture(gesture)
    #endif
  }
}

/// Represents a view displaying a historical graph.
struct HistoryGraph: View {
  /// The price history data consisting of dates and corresponding prices.
  let history: [Date: NSNumber]
  let currency: String
  let graphAccessibilityLabel: String

  /// Graph gradient color.
  static let graphGradientColor = "graph_gradient_color"

  /// Color representing blue (600).
  static let blue600 = UIColor(named: kBlue600Color) ?? .blue

  /// Color representing solid white.
  static let backgroundColor = UIColor(named: kBackgroundColor) ?? .white

  /// Color representing grey 200.
  static let grey200 = UIColor(named: kGrey200Color) ?? .gray

  /// Number of ticks on the Y-axis.
  static let tickCountY = 3

  /// The selected date on the graph.
  @State private var selectedDate: Date?

  /// The horizontal x position of the current selection on the chart.
  @State private var selectedXPosition: CGFloat?

  /// The width of the entire chart.
  @State private var chartWidth: CGFloat?

  /// Indicates whether the graph has been interacted with for the current session.
  @State private var hasGraphInteracted: Bool = false

  /// Color scheme environment value .
  @Environment(\.colorScheme) var colorScheme

  var body: some View {
    /// A linear gradient used for styling the graph.
    let linearGradient = LinearGradient(
      gradient: Gradient(colors: [
        Color(Self.graphGradientColor).opacity(colorScheme == .dark ? 0.2 : 0.4),
        Color.clear,
      ]),
      startPoint: .top,
      endPoint: .bottom)

    /// Sort price history data to render them in graph.
    let sortedHistoryDates = Array(history.sorted(by: { $0.key < $1.key }))
    let sortedHistoryPrice = Array(
      history.values.sorted { $0.doubleValue < $1.doubleValue })

    /// Calculating axis ticks and range.
    let (axisTicksY, axisYRange) = getYAxisTicksInfo(prices: sortedHistoryPrice)
    let axisXRange =
      (sortedHistoryDates.first?.key ?? Date())...(sortedHistoryDates.last?.key ?? Date())

    /// TODO(b/333894542): Configure audio graph for accessibility and ensure labels
    /// for line marks and rule marks are accessible.
    Chart {
      ForEach(sortedHistoryDates, id: \.key) { date, price in
        /// Displaying the area mark under the line mark.
        AreaMark(
          x: .value("Date", date),
          yStart: .value("Minimun price in range", axisTicksY.first ?? 0),
          yEnd: .value("Price", price.doubleValue)
        )
        .foregroundStyle(linearGradient)
        .accessibilityHidden(true)

        // Displaying the line mark on the graph.
        LineMark(
          x: .value("Date", date),
          y: .value("Price", price.doubleValue)
        ).foregroundStyle(Color(uiColor: Self.blue600))
      }
      .interpolationMethod(.stepEnd)

      /// Displaying the dashed line and point mark for selected date on the graph.
      if let selectedDate = selectedDate, let selectedPrice = history[selectedDate] {
        RuleMark(
          x: .value("Date", selectedDate)
        )
        .lineStyle(StrokeStyle(lineWidth: 1, dash: [3]))
        .accessibilityHidden(true)

        PointMark(
          x: .value("Date", selectedDate),
          y: .value("Price", selectedPrice.doubleValue)
        )
        .symbol {
          Circle()
            .fill(Color(uiColor: Self.blue600))
            .frame(width: 8, height: 8)
            .overlay(
              Circle()
                .stroke(Color(uiColor: Self.backgroundColor), lineWidth: 2)
            )
        }
        .foregroundStyle(Color(uiColor: Self.blue600))
        .accessibilityHidden(true)
      }
    }
    .chartBackground { chartProxy in
      Color(uiColor: Self.backgroundColor)
    }
    .chartYScale(domain: axisYRange)
    .chartYAxis {
      /// Setting up Y-axis.
      AxisMarks(position: .leading, values: axisTicksY) { price in
        if let price = price.as(Double.self) {
          if price == axisTicksY.first {
            AxisTick(length: .longestLabel, stroke: StrokeStyle(lineWidth: 1))
              .foregroundStyle(Color(uiColor: Self.grey200))
          } else {
            AxisValueLabel(
              format: .currency(code: currency).precision(.fractionLength(0)))
            AxisTick(stroke: StrokeStyle(lineWidth: 0))
          }
        }
        AxisGridLine(stroke: StrokeStyle(lineWidth: 1))
          .foregroundStyle(Color(uiColor: Self.grey200))
      }
    }
    .chartXScale(domain: axisXRange)
    .chartXAxis {
      AxisMarks(preset: .aligned, stroke: StrokeStyle(lineWidth: 0))
    }
    .chartOverlay { proxy in
      /// Gesture for selecting date on the graph.
      GeometryReader { geometry in
        Rectangle().fill(.clear).contentShape(Rectangle())
          .onAppear {
            if selectedDate == nil {
              selectedDate = sortedHistoryDates.last?.key
              updateTooltipPosition(geometry: geometry, chart: proxy)
            }
          }
          .onContinuousHover(perform: { phase in
            switch phase {
            case .active(let location):
              updateSelectionData(location: location, geometry: geometry, chart: proxy)
              updateTooltipPosition(geometry: geometry, chart: proxy)
            case .ended:
              recordGraphInteraction()
              break
            }
          })
          .modifier(
            GraphGesture(
              geometry: geometry,
              proxy: proxy,
              updateSelectionData: updateSelectionData,
              updateTooltipPosition: updateTooltipPosition,
              recordGraphInteraction: recordGraphInteraction
            ))
      }
    }
    .overlay(
      Group {
        if let date = selectedDate, let price = history[date],
          let xPosition = selectedXPosition, let chartWidth = chartWidth
        {
          TooltipView(
            currency: currency,
            price: price.doubleValue,
            date: date,
            xPosition: xPosition,
            chartWidth: chartWidth
          )
        }
      }
      .accessibilityHidden(true)
    )
    .edgesIgnoringSafeArea(.all)
    .accessibilityLabel(graphAccessibilityLabel)
  }

  /// Updates the selected data when the given `location` is selected inside the given
  /// `geometry` and `chart`.
  private func updateSelectionData(location: CGPoint, geometry: GeometryProxy, chart: ChartProxy) {
    let startX = geometry[chart.plotAreaFrame].origin.x
    let currentX = location.x - startX
    if let index: Date = chart.value(atX: currentX) {
      selectedDate = closestDate(to: index, in: history)
    }
  }

  // Records user interactions with the history graph.
  private func recordGraphInteraction() {
    if !hasGraphInteracted {
      hasGraphInteracted = true
      UserMetricsUtils.recordAction("Commerce.PriceInsights.HistoryGraphInteraction")
    }
  }

  /// Calculates and updates the tooltip's position based on the selected date
  /// and chart geometry.
  private func updateTooltipPosition(geometry: GeometryProxy, chart: ChartProxy) {
    if let selectedDate = selectedDate {
      let startX = geometry[chart.plotAreaFrame].origin.x
      if let xPosition = chart.position(forX: selectedDate) {
        selectedXPosition = xPosition + startX
      }
    }
    chartWidth = geometry.size.width
  }

  /// Finds the closest date to the given date from the price history dictionary.
  private func closestDate(to date: Date, in dictionary: [Date: NSNumber]) -> Date? {
    return dictionary.keys.min(by: {
      abs($0.timeIntervalSince(date)) < abs($1.timeIntervalSince(date))
    })
  }

  /// Calculates and returns information about the Y-axis ticks
  /// based on the provided sorted array of prices, adjusting them to ensure
  /// readability and an appropriate range for the Y-axis.
  private func getYAxisTicksInfo(prices: [NSNumber]) -> (
    ticks: [Double], range: ClosedRange<Double>
  ) {
    /// Minimum and maximum prices.
    var paddedMinPrice = prices.first?.doubleValue ?? 0
    var paddedMaxPrice = prices.last?.doubleValue ?? 0

    /// Median price.
    let medianIndex = ceil(Double(prices.count / 2))
    let medianPrice = prices[Int(medianIndex)].doubleValue

    /// Calculate the padding for the prices
    let padding = max(medianPrice / 10, 1)
    paddedMinPrice = max(paddedMinPrice - padding, 0)
    paddedMaxPrice += padding

    let valueRange = paddedMaxPrice - paddedMinPrice
    var tickInterval = valueRange / Double(Self.tickCountY - 1)
    var tickLow = paddedMinPrice

    /// Ensure the tick interval is a multiple of below values to improve the
    /// readability. Bigger values are used when possible.
    let multipliers = [100.0, 50.0, 20.0, 10.0, 5.0, 2.0, 1.0]
    if let multiplier = multipliers.first(where: { tickInterval >= 2 * $0 }) {
      tickInterval = ceil(tickInterval / multiplier) * multiplier

      /// Calculate the lowest tick value
      tickLow = paddedMinPrice - (tickInterval * Double(Self.tickCountY - 1) - valueRange) / 2
      tickLow = floor(tickLow / multiplier) * multiplier
      tickLow = max(tickLow, 0)
      tickInterval =
        ceil((paddedMaxPrice - tickLow) / Double(Self.tickCountY - 1) / multiplier) * multiplier
    }

    let ticks = (-1..<Self.tickCountY).map { tickLow + Double($0) * tickInterval }

    let rangeTail = (ticks.last ?? 0.0) + (tickInterval / 2)
    return (ticks, (ticks.first ?? 0.0)...rangeTail)
  }
}
