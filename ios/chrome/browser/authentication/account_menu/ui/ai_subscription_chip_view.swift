// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This codes is copied as-is from Google repository where it is implemented in Swift.

import SwiftUI
import UIKit

/// A chip view that indicates that the user has an AI subscription.
public struct AISubscriptionChipView: View {
  private enum ViewConstants {
    static let spacing = 2.0
    static let horizontalPadding = 8.0
    static let verticalPadding = 5.0
    static let cornerRadius = 36.0
    static let strokeInset = 0.5
    static let topPadding = 0.0  // Adjusted since UIKit will handle spacing below avatar
    static let overlayHeight = 8.0
    static let overlayBlurRadius = 2.5
  }

  private let text: String
  private let foregroundColor: Color
  private let backgroundColor: Color

  public init(text: String, foregroundColor: Color? = nil, backgroundColor: Color? = nil) {
    self.text = text
    self.foregroundColor = foregroundColor ?? .onSurface
    self.backgroundColor = backgroundColor ?? .surfaceContainer
  }

  public var body: some View {
    HStack(alignment: .center, spacing: ViewConstants.spacing) {
      Text(text)
        .font(.aiSubscriptionLabelBold)
        .foregroundStyle(foregroundColor)
        .multilineTextAlignment(.center)
        .fixedSize(horizontal: true, vertical: false)
        .dynamicTypeSize(.large)
    }
    .padding(.horizontal, ViewConstants.horizontalPadding)
    .padding(.vertical, ViewConstants.verticalPadding)
    .background(
      AISubscriptionGradientView(
        cornerRadius: ViewConstants.cornerRadius, backgroundColor: backgroundColor)
    ).overlay(alignment: .bottom) {
      SubscriptionBottomWave()
        .fill(
          LinearGradient(
            stops: Color.aiSubscriptionBottomWaveGradientStops,
            startPoint: .bottomLeading,
            endPoint: .topTrailing
          )
        )
        .frame(height: ViewConstants.overlayHeight)
        .blur(radius: ViewConstants.overlayBlurRadius)
    }
    .cornerRadius(ViewConstants.cornerRadius)
    .padding(.top, ViewConstants.topPadding)
  }

  private struct AISubscriptionGradientView: View {
    private enum ViewConstants {
      static let lineWidth = 3.5
      static let blurRadius = 2.0
      static let offset = 1.5
    }

    var cornerRadius: CGFloat
    var backgroundColor: Color

    var body: some View {
      RoundedRectangle(cornerRadius: cornerRadius)
        .fill(.ultraThinMaterial)
        .overlay(
          RoundedRectangle(cornerRadius: cornerRadius)
            .fill(Color.aiSubscriptionPillGlow)
        )
        .overlay(
          RoundedRectangle(cornerRadius: cornerRadius)
            .stroke(
              LinearGradient(
                stops: Color.aiSubscriptionPillGradientStops,
                startPoint: .bottomLeading,
                endPoint: .topTrailing
              ),
              lineWidth: ViewConstants.lineWidth
            )
            .blur(radius: ViewConstants.blurRadius)
            .offset(x: ViewConstants.offset, y: -ViewConstants.offset)
            .background(backgroundColor)
            .mask(RoundedRectangle(cornerRadius: cornerRadius))
        )
    }
  }
}

// Custom Colors extension for AI subscription indicators
extension Color {
  public static let onSurface = Color(uiColor: .label)
  public static let surfaceContainer = Color(uiColor: .secondarySystemBackground)

  public static let aiSubscriptionPillGlow = Color(
    red: 252 / 255, green: 252 / 255, blue: 252 / 255
  ).opacity(0.9)

  public static let aiSubscriptionPillGradientStops: [Gradient.Stop] = [
    .init(color: Color(red: 0.19, green: 0.53, blue: 1.0, opacity: 0.85), location: 0.0),
    .init(color: Color(red: 0.19, green: 0.53, blue: 1.0, opacity: 0.20), location: 1.0),
  ]

  public static let aiSubscriptionBottomWaveGradientStops: [Gradient.Stop] = [
    .init(color: Color(red: 0.64, green: 0.65, blue: 1.0), location: 0.51),
    .init(color: Color(red: 0.35, green: 0.62, blue: 1.0), location: 0.676),
    .init(color: Color(red: 0.21, green: 0.42, blue: 0.93), location: 0.759),
    .init(color: Color(red: 0.21, green: 0.42, blue: 0.93), location: 0.892),
  ]
}

// Custom Font extension for AI subscription indicators
extension Font {
  public static let aiSubscriptionLabelBold = Font.system(size: 12, weight: .bold)
}

/// A wrapper UIView that incorporates AISubscriptionChipView.
@objc(AISubscriptionChipWrapperView)
public class AISubscriptionChipWrapperView: UIView {
  private var hostingController: UIHostingController<AISubscriptionChipView>?

  @objc
  public init(text: String) {
    super.init(frame: .zero)
    let chipView = AISubscriptionChipView(text: text)
    let hostingController = UIHostingController(rootView: chipView)
    self.hostingController = hostingController

    let childView = hostingController.view!
    childView.translatesAutoresizingMaskIntoConstraints = false
    childView.backgroundColor = .clear
    addSubview(childView)

    NSLayoutConstraint.activate([
      childView.leadingAnchor.constraint(equalTo: leadingAnchor),
      childView.trailingAnchor.constraint(equalTo: trailingAnchor),
      childView.topAnchor.constraint(equalTo: topAnchor),
      childView.bottomAnchor.constraint(equalTo: bottomAnchor),
    ])
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }
}
