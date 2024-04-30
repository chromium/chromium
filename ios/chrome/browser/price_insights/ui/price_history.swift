// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Charts
import Foundation
import SwiftUI

/// Represents a view displaying a historical graph.
struct HistoryGraph: View {
  /// The price history data consisting of dates and corresponding prices.
  let history: [Date: NSNumber]
  let currency: String

  /// Color representing blue (300).
  static let blue300 = UIColor(named: kBlue300Color) ?? .blue

  /// Color representing blue (600).
  static let blue600 = UIColor(named: kBlue600Color) ?? .blue

  /// Color representing solid white.
  static let solidWhite = UIColor(named: kSolidWhiteColor) ?? .white

  /// Number of ticks on the Y-axis.
  static let tickCountY = 4

  /// The selected date on the graph.
  @State private var selectedDate: Date?

  /// Color scheme environment value .
  @Environment(\.colorScheme) var colorScheme

  var body: some View {
    /// A linear gradient used for styling the graph.
    let linearGradient = LinearGradient(
      gradient: Gradient(colors: [
        Color(uiColor: Self.blue300).opacity(colorScheme == .dark ? 0.2 : 0.4),
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
      /// Displaying the dashed line and point mark for selected date on the graph.
      /// TODO(b/333894032): Add a tooltip displaying the price and date.
      if let selectedDate = selectedDate, let selectedPrice = history[selectedDate] {
        RuleMark(
          x: .value("Date", selectedDate)
        )
        .lineStyle(StrokeStyle(lineWidth: 1, dash: [3]))
        PointMark(
          x: .value("Date", selectedDate),
          y: .value("Price", selectedPrice.doubleValue)
        )
        .foregroundStyle(Color(uiColor: Self.blue600))
      }

      ForEach(sortedHistoryDates, id: \.key) { date, price in
        // Displaying the line mark on the graph.
        LineMark(
          x: .value("Date", date),
          y: .value("Price", price.doubleValue)
        ).foregroundStyle(Color(uiColor: Self.blue600))

        /// Displaying the area mark under the line mark.
        AreaMark(
          x: .value("Date", date),
          yStart: .value("Minimun price in range", axisTicksY.first ?? 0),
          yEnd: .value("Price", price.doubleValue)
        )
        .foregroundStyle(linearGradient)
      }
      .interpolationMethod(.stepEnd)
    }
    .chartBackground { chartProxy in
      Color(uiColor: Self.solidWhite)
    }
    .chartYScale(domain: axisYRange)
    .chartYAxis {
      /// Setting up Y-axis.
      AxisMarks(position: .leading, values: axisTicksY) { price in
        if let price = price.as(Double.self) {
          if price == axisTicksY.first {
            AxisTick()
          } else {
            AxisValueLabel(
              format: .currency(code: currency).precision(.fractionLength(0)))
            AxisTick(stroke: StrokeStyle(lineWidth: 0))
          }
        }
        AxisGridLine()
      }
    }
    /// TODO(b/334988024): Polish chartXAxis y adding chartXAxis.
    .chartXScale(domain: axisXRange)
    .chartOverlay { proxy in
      /// Gesture for selecting date on the graph.
      GeometryReader { geometry in
        Rectangle().fill(.clear).contentShape(Rectangle())
          .gesture(
            DragGesture()
              .onChanged { value in
                let startX = geometry[proxy.plotAreaFrame].origin.x
                let currentX = value.location.x - startX
                if let index: Date = proxy.value(atX: currentX) {
                  selectedDate = closestDate(to: index, in: history)
                }
              }
              .onEnded { _ in
                selectedDate = nil
              }
          )
      }
    }
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
    var tickLow = 0.0

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
