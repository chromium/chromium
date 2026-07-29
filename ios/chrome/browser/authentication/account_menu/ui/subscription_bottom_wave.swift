// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Origin: google3/googlemac/iPhone/Shared/Identity/Common/UI/AISubscriptionIndicators/SubscriptionBottomWave.swift

import SwiftUI

/// A shape that represents the wave at the bottom of the AI subscription indicator.
struct SubscriptionBottomWave: Shape {
  private enum ShapeConstants {
    static let designWidth = 59.0
    static let designHeight = 11.0

    // Design coordinates for the path.
    // The design coordinates extend slightly beyond designWidth (up to 60.0) from 0.0
    // to intentionally overshoot layout boundaries and prevent rendering artifacts or gaps.
    static let startPoint = CGPoint(x: 20.5337, y: 6.94302)
    static let firstCurveStart = CGPoint(x: 18.5045, y: 7.81268)
    static let firstCurveEnd = CGPoint(x: 13.2752, y: 8.88605)
    static let firstCurveControl1 = CGPoint(x: 16.8521, y: 8.52088)
    static let firstCurveControl2 = CGPoint(x: 15.073, y: 8.88605)
    static let segmentLeftEnd = CGPoint(x: 1.0, y: 8.88605)

    static let cornerLeftStart = CGPoint(x: 0.0, y: 9.88605)
    static let cornerLeftControl1 = CGPoint(x: 0.447715, y: 8.88605)
    static let cornerLeftControl2 = CGPoint(x: 0.0, y: 9.33376)

    static let cornerLeftEnd = CGPoint(x: 1.0, y: 10.886)
    static let cornerLeftEndControl1 = CGPoint(x: 0.0, y: 10.4383)
    static let cornerLeftEndControl2 = CGPoint(x: 0.447715, y: 10.886)

    static let bottomCenter = CGPoint(x: 30.0, y: 10.886)
    static let bottomRight = CGPoint(x: 59.0, y: 10.886)

    static let cornerRightStart = CGPoint(x: 60.0, y: 9.88605)
    static let cornerRightControl1 = CGPoint(x: 59.5523, y: 10.886)
    static let cornerRightControl2 = CGPoint(x: 60.0, y: 10.4383)

    static let cornerRightEnd = CGPoint(x: 59.0, y: 8.88605)
    static let cornerRightEndControl1 = CGPoint(x: 60.0, y: 9.33376)
    static let cornerRightEndControl2 = CGPoint(x: 59.5523, y: 8.88605)

    static let segmentRightEnd = CGPoint(x: 46.7248, y: 8.88605)

    static let secondCurveEnd = CGPoint(x: 41.4955, y: 7.81268)
    static let secondCurveControl1 = CGPoint(x: 44.927, y: 8.88605)
    static let secondCurveControl2 = CGPoint(x: 43.1479, y: 8.52088)

    static let secondCurveStart = CGPoint(x: 39.4663, y: 6.94302)

    static let mainCurveControl1 = CGPoint(x: 33.4213, y: 4.35232)
    static let mainCurveControl2 = CGPoint(x: 26.5787, y: 4.35232)
  }

  func path(in rect: CGRect) -> Path {
    guard rect.width > 0 && rect.height > 0 else { return Path() }

    var path = Path()

    // Scale X to the dynamic width of the HStack.
    let scaleX = rect.width / ShapeConstants.designWidth
    // Scale Y to the fixed height.
    let scaleY = rect.height / ShapeConstants.designHeight

    // Helper to map coordinate values from design-space (up to 60x11) to the actual view size.
    // The design coordinates extend slightly beyond designWidth (up to 60.0) from 0.0
    // to intentionally overshoot layout boundaries and prevent rendering artifacts or gaps.
    func p(_ point: CGPoint) -> CGPoint {
      CGPoint(x: point.x * scaleX, y: point.y * scaleY)
    }

    path.move(to: p(ShapeConstants.startPoint))
    path.addLine(to: p(ShapeConstants.firstCurveStart))
    path.addCurve(
      to: p(ShapeConstants.firstCurveEnd),
      control1: p(ShapeConstants.firstCurveControl1),
      control2: p(ShapeConstants.firstCurveControl2)
    )
    path.addLine(to: p(ShapeConstants.segmentLeftEnd))
    path.addCurve(
      to: p(ShapeConstants.cornerLeftStart),
      control1: p(ShapeConstants.cornerLeftControl1),
      control2: p(ShapeConstants.cornerLeftControl2)
    )
    path.addCurve(
      to: p(ShapeConstants.cornerLeftEnd),
      control1: p(ShapeConstants.cornerLeftEndControl1),
      control2: p(ShapeConstants.cornerLeftEndControl2)
    )
    path.addLine(to: p(ShapeConstants.bottomCenter))
    path.addLine(to: p(ShapeConstants.bottomRight))
    path.addCurve(
      to: p(ShapeConstants.cornerRightStart),
      control1: p(ShapeConstants.cornerRightControl1),
      control2: p(ShapeConstants.cornerRightControl2)
    )
    path.addCurve(
      to: p(ShapeConstants.cornerRightEnd),
      control1: p(ShapeConstants.cornerRightEndControl1),
      control2: p(ShapeConstants.cornerRightEndControl2)
    )
    path.addLine(to: p(ShapeConstants.segmentRightEnd))
    path.addCurve(
      to: p(ShapeConstants.secondCurveEnd),
      control1: p(ShapeConstants.secondCurveControl1),
      control2: p(ShapeConstants.secondCurveControl2)
    )
    path.addLine(to: p(ShapeConstants.secondCurveStart))
    path.addCurve(
      to: p(ShapeConstants.startPoint),
      control1: p(ShapeConstants.mainCurveControl1),
      control2: p(ShapeConstants.mainCurveControl2)
    )
    path.closeSubpath()

    return path
  }
}
