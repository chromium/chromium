// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// Style based on state for an OverflowMenuDestinationView.
struct OverflowMenuDestinationButton: ButtonStyle {
  enum Dimensions {
    static let cornerRadius: CGFloat = 13

    /// The padding on either side of the text in the vertical layout,
    /// separating it from the next view.
    static let verticalLayoutTextPadding: CGFloat = 3

    /// The padding on either side of the view in the horizontal layout,
    /// separating it from the next view.
    static let horizontalLayoutViewPadding: CGFloat = 16

    /// The padding around the icon and inside the background in horizontal
    /// layout.
    static let horizontalLayoutIconPadding: CGFloat = 3

    /// The spacing between the icon and the text in horizontal layout.
    static let horizontalLayoutIconSpacing: CGFloat = 14

    /// The image width, which controls the width of the overall view.
    static let imageWidth: CGFloat = 54
  }

  /// The destination for this view.
  var destination: OverflowMenuDestination

  /// The layout parameters for this view.
  var layoutParameters: OverflowMenuDestinationView.LayoutParameters

  func makeBody(configuration: Configuration) -> some View {
    Group {
      switch layoutParameters {
      case .vertical(let iconSpacing, let iconPadding):
        VStack {
          icon(configuration: configuration)
          text
        }
        .frame(width: Dimensions.imageWidth + 2 * iconSpacing + 2 * iconPadding)
      case .horizontal(let itemWidth):
        HStack {
          icon(configuration: configuration)
          Spacer().frame(width: Dimensions.horizontalLayoutIconSpacing)
          text
        }
        .frame(width: itemWidth, alignment: .leading)
        // In horizontal layout, the item itself has leading and trailing
        // padding.
        .padding([.leading, .trailing], Dimensions.horizontalLayoutViewPadding)
      }
    }
    .contentShape(Rectangle())
  }

  /// View representing the background of the icon.
  func iconBackground(configuration: Configuration) -> some View {
    RoundedRectangle(cornerRadius: Dimensions.cornerRadius)
      .foregroundColor(configuration.isPressed ? .cr_grey300 : .cr_groupedSecondaryBackground)
  }

  /// Icon for the destination.
  func icon(configuration: Configuration) -> some View {
    let interiorPadding: CGFloat
    let spacing: CGFloat
    switch layoutParameters {
    case .vertical(let iconSpacing, let iconPadding):
      spacing = iconSpacing
      interiorPadding = iconPadding
    case .horizontal:
      spacing = 0
      interiorPadding = Dimensions.horizontalLayoutIconPadding
    }
    return destination.image
      .padding(interiorPadding)
      .background(iconBackground(configuration: configuration))
      .padding([.leading, .trailing], spacing)
      // Without explicitly removing the image from accessibility,
      // VoiceOver will occasionally read out icons it thinks it can
      // recognize.
      .accessibilityHidden(true)
  }

  /// Text view for the destination.
  var text: some View {
    // Only the vertical layout has extra spacing around the text
    let textSpacing: CGFloat
    let maximumLines: Int?
    switch layoutParameters {
    case .vertical:
      textSpacing = Dimensions.verticalLayoutTextPadding
      maximumLines = nil
    case .horizontal:
      textSpacing = 0
      maximumLines = 1
    }
    return Text(destination.name)
      .font(.caption2)
      .padding([.leading, .trailing], textSpacing)
      .multilineTextAlignment(.center)
      .lineLimit(maximumLines)
  }
}

/// A view displaying a single destination.
struct OverflowMenuDestinationView: View {

  /// Parameters providing any necessary data to layout the view.
  enum LayoutParameters {
    /// The destination has an icon on top and text below.
    /// There is `iconSpacing` to either side of the icon, and `iconPadding`
    /// around the icon and inside the background.
    case vertical(iconSpacing: CGFloat, iconPadding: CGFloat)
    /// The destination has an icon on the left and text on the right. Here
    /// the view will have a fixed overall `itemWidth`.
    case horizontal(itemWidth: CGFloat)
  }

  /// The destination for this view.
  var destination: OverflowMenuDestination

  /// The layout parameters for this view.
  var layoutParameters: LayoutParameters

  var body: some View {
    Button(
      action: destination.handler,
      label: {
        EmptyView()
      }
    )
    .buttonStyle(
      OverflowMenuDestinationButton(destination: destination, layoutParameters: layoutParameters))
  }
}
