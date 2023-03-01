// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// Style based on state for an OverflowMenuDestinationView.
@available(iOS 15, *)
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

    /// The size of the Symbol in the icon.
    static let iconSymbolSize: CGFloat = 26

    /// The width of the icon, used for positioning the unread badge over the
    /// corner.
    static let iconWidth: CGFloat = 30

    /// The width of the badge circle.
    static let badgeWidth: CGFloat = 10

    /// The width of the new label badge.
    static let newLabelBadgeWidth: CGFloat = 20
  }

  /// The destination for this view.
  var destination: OverflowMenuDestination

  /// The layout parameters for this view.
  var layoutParameters: OverflowMenuDestinationView.LayoutParameters

  weak var metricsHandler: PopupMenuMetricsHandler?

  func makeBody(configuration: Configuration) -> some View {
    let destinationWidth = OverflowMenuDestinationButton.destinationWidth(
      forLayoutParameters: layoutParameters)
    Group {
      switch layoutParameters {
      case .vertical:
        VStack {
          icon(configuration: configuration)
          text
        }
        .frame(width: destinationWidth)
      case .horizontal:
        HStack {
          icon(configuration: configuration)
          Spacer().frame(width: Dimensions.horizontalLayoutIconSpacing)
          text
        }
        .frame(width: destinationWidth, alignment: .leading)
        // In horizontal layout, the item itself has leading and trailing
        // padding.
        .padding([.leading, .trailing], Dimensions.horizontalLayoutViewPadding)
      }
    }
    .contentShape(Rectangle())
  }

  /// Background color for the icon.
  func backgroundColor(configuration: Configuration) -> Color {
    return configuration.isPressed ? Color(.systemGray4) : .groupedSecondaryBackground
  }

  /// View representing the background of the icon.
  func iconBackground(configuration: Configuration) -> some View {
    RoundedRectangle(cornerRadius: Dimensions.cornerRadius)
      .foregroundColor(backgroundColor(configuration: configuration))
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
    let image: Image
    if !destination.symbolName.isEmpty {
      image =
        (destination.systemSymbol
        ? Image(systemName: destination.symbolName) : Image(destination.symbolName)).renderingMode(
          .template)
    } else {
      image = destination.image
    }
    return iconBuilder(
      configuration: configuration, spacing: spacing, interiorPadding: interiorPadding, image: image
    )
  }

  var newBadgeOffsetX: CGFloat {
    return Dimensions.iconWidth - (Dimensions.newLabelBadgeWidth - 10)
  }

  var newBadgeOffsetY: CGFloat {
    return -Dimensions.iconWidth + (Dimensions.newLabelBadgeWidth - 10)
  }

  /// Build the image to be displayed, based on the configuration of the item.
  /// TODO(crbug.com/1315544): Remove this once only the symbols are present.
  @ViewBuilder
  func iconBuilder(
    configuration: Configuration, spacing: CGFloat, interiorPadding: CGFloat, image: Image
  ) -> some View {
    let configuredImage =
      image
      .overlay {
        if destination.badge == .blueDot {
          Circle()
            .foregroundColor(.blue600)
            .frame(width: Dimensions.badgeWidth, height: Dimensions.badgeWidth)
            .offset(
              x: Dimensions.iconWidth - (Dimensions.badgeWidth / 2),
              y: -Dimensions.iconWidth + (Dimensions.badgeWidth / 2))
        } else if destination.badge == .newLabel {
          Image(systemName: "seal.fill")
            .resizable()
            .foregroundColor(.blue600)
            .frame(width: Dimensions.newLabelBadgeWidth, height: Dimensions.newLabelBadgeWidth)
            .offset(x: newBadgeOffsetX, y: newBadgeOffsetY)
            .overlay {
              if let newLabelString = L10NUtils.stringWithFixup(
                forMessageId: IDS_IOS_NEW_LABEL_FEATURE_BADGE)
              {
                Text(newLabelString)
                  .font(.system(size: 10, weight: .bold, design: .rounded))
                  .offset(x: newBadgeOffsetX, y: newBadgeOffsetY)
                  .scaledToFit()
                  .foregroundColor(.primaryBackground)
              }
            }
        }
      }
      .frame(width: Dimensions.imageWidth, height: Dimensions.imageWidth)
      .padding(interiorPadding)
      .background(iconBackground(configuration: configuration))
      .padding([.leading, .trailing], spacing)
      // Without explicitly removing the image from accessibility,
      // VoiceOver will occasionally read out icons it thinks it can
      // recognize.
      .accessibilityHidden(true)

    if !destination.symbolName.isEmpty {
      configuredImage
        .foregroundColor(.blue600).imageScale(.medium).font(
          Font.system(size: Dimensions.iconSymbolSize, weight: .medium))
    } else {
      configuredImage
    }
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

  static public func destinationWidth(
    forLayoutParameters layoutParameters: OverflowMenuDestinationView.LayoutParameters
  ) -> CGFloat {
    switch layoutParameters {
    case .vertical(let iconSpacing, let iconPadding):
      return Dimensions.imageWidth + 2 * iconSpacing + 2 * iconPadding
    case .horizontal(let itemWidth):
      return itemWidth
    }
  }
}

/// A view displaying a single destination.
@available(iOS 15, *)
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

  enum AccessibilityIdentifier {
    /// The addition to the `accessibilityIdentfier` for this element if it
    /// has a badge.
    static let badgeAddition = "badge"

    /// The addition to the `accessibilityIdentfier` for this element if it
    /// has a "New" badge.
    static let newBadgeAddition = "newBadge"
  }

  /// The destination for this view.
  var destination: OverflowMenuDestination

  /// The layout parameters for this view.
  var layoutParameters: LayoutParameters

  weak var metricsHandler: PopupMenuMetricsHandler?

  var body: some View {
    Button(
      action: {
        metricsHandler?.popupMenuTookAction()
        destination.handler()
      },
      label: {
        EmptyView()
      }
    )
    .accessibilityIdentifier(accessibilityIdentifier)
    .accessibilityLabel(Text(accessibilityLabel))
    .buttonStyle(
      OverflowMenuDestinationButton(destination: destination, layoutParameters: layoutParameters))
  }

  var accessibilityLabel: String {
    return [
      destination.name,
      destination.badge == .blueDot
        ? L10NUtils.stringWithFixup(forMessageId: IDS_IOS_NEW_ITEM_ACCESSIBILITY_HINT) : nil,
      destination.badge == .newLabel
        ? L10NUtils.stringWithFixup(forMessageId: IDS_IOS_TOOLS_MENU_CELL_NEW_FEATURE_BADGE) : nil,
    ].compactMap { $0 }.joined(separator: ", ")
  }

  var accessibilityIdentifier: String {
    return [
      destination.accessibilityIdentifier,
      destination.badge == .blueDot ? AccessibilityIdentifier.badgeAddition : nil,
      destination.badge == .newLabel ? AccessibilityIdentifier.newBadgeAddition : nil,
    ].compactMap { $0 }.joined(separator: "-")
  }

}
