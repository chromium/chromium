// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_browser_shared_ui_util_util_swiftui
import ios_chrome_common_ui_colors_swift

/// `ButtonStyle` that communicates the button's `isPressed` state back to the
/// parent.
struct IsPressedStyle: ButtonStyle {
  @Binding var isPressed: Bool

  @ViewBuilder
  func makeBody(configuration: Configuration) -> some View {
    configuration.label
      .onChange(of: configuration.isPressed) { newValue in
        isPressed = newValue
      }
  }
}

/// `PreferenceKey` holding the frame of the icon in the destination view.
struct IconFramePreferenceKey: PreferenceKey {
  static var defaultValue: CGRect = .null
  static func reduce(value: inout CGRect, nextValue: () -> CGRect) {
    value = CGRectUnion(value, nextValue())
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
    case horizontal
  }

  /// Shape consisting of a path around the icon and text.
  struct IconShape: Shape {
    let iconFrame: CGRect

    func path(in rect: CGRect) -> Path {
      return Path(roundedRect: iconFrame, cornerRadius: Dimensions.cornerRadius)
    }
  }

  enum AccessibilityIdentifier {
    /// The addition to the `accessibilityIdentfier` for this element if it
    /// has an error badge.
    static let errorBadge = "errorBadge"

    /// The addition to the `accessibilityIdentfier` for this element if it
    /// has a promo badge.
    static let promoBadge = "promoBadge"

    /// The addition to the `accessibilityIdentfier` for this element if it
    /// has a "New" badge.
    static let newBadge = "newBadge"
  }

  enum Dimensions {
    static let cornerRadius: CGFloat = 13

    /// The padding on either side of the text in the vertical layout,
    /// separating it from the next view.
    static let verticalLayoutTextPadding: CGFloat = 3

    /// The padding on either side of the view in the horizontal layout,
    /// separating it from the next view.
    static let horizontalLayoutViewPadding: CGFloat = 13

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

    /// The top padding of the hover effect on destination items.
    static let hoverEffectTopPadding: CGFloat = 10

    /// The bottom padding of the hover effect on destination items.
    static let hoverEffectBottomPadding: CGFloat = 3
  }

  static let viewNamespace = "destinationView"

  /// The destination for this view.
  var destination: OverflowMenuDestination

  /// The layout parameters for this view.
  var layoutParameters: LayoutParameters

  var highlighted = false

  @Environment(\.editMode) var editMode

  @State private var isPressed = false

  @State private var iconFrame: CGRect = .zero

  weak var metricsHandler: PopupMenuMetricsHandler?

  var body: some View {
    button
      .coordinateSpace(name: Self.viewNamespace)
      .contentShape(
        [.contextMenuPreview, .dragPreview],
        IconShape(iconFrame: iconFrame)
      )
      .if(editMode?.wrappedValue.isEditing != true) { view in
        view.contextMenu {
          ForEach(destination.longPressItems) { item in
            Section {
              Button {
                item.handler()
              } label: {
                Label(item.title, systemImage: item.symbolName)
              }
            }
          }
        }
      }
      .accessibilityIdentifier(accessibilityIdentifier)
      .accessibilityLabel(Text(accessibilityLabel))
      .if(highlighted) { view in
        view.anchorPreference(
          key: OverflowMenuDestinationList.HighlightedDestinationBounds.self, value: .bounds
        ) { $0 }
      }
      .onPreferenceChange(IconFramePreferenceKey.self) { newFrame in
        iconFrame = newFrame
      }
  }

  // The button view, which is replaced by just a plain view when this is in
  // edit mode.
  @ViewBuilder
  var button: some View {
    if editMode?.wrappedValue.isEditing == true {
      buttonContent
    } else {
      ZStack(alignment: .top) {
        RoundedRectangle(cornerRadius: Dimensions.cornerRadius)
          .opacity(0)
        Button(
          action: {
            metricsHandler?.popupMenuTookAction()
            metricsHandler?.popupMenuUserSelectedDestination()
            destination.handler()
          },
          label: {
            buttonContent
          }
        )
        .buttonStyle(IsPressedStyle(isPressed: $isPressed))
      }
      .padding(.top, Dimensions.hoverEffectTopPadding)
      .padding(.bottom, Dimensions.hoverEffectBottomPadding)
      .contentShape(
        .hoverEffect,
        RoundedRectangle(cornerRadius: Dimensions.cornerRadius)
      )
      .hoverEffect(.automatic)
    }
  }

  /// The content of the button view.
  @ViewBuilder
  var buttonContent: some View {
    Group {
      switch layoutParameters {
      case .vertical(let iconSpacing, let iconPadding):
        VStack {
          icon
          text
        }
        .frame(
          width: Self.verticalLayoutDestinationWidth(
            iconSpacing: iconSpacing, iconPadding: iconPadding))
      case .horizontal:
        HStack {
          icon
          Spacer().frame(width: Dimensions.horizontalLayoutIconSpacing)
          text
        }
        // In horizontal layout, the item itself has leading and trailing
        // padding.
        .padding([.leading, .trailing], Dimensions.horizontalLayoutViewPadding)
      }
    }
    .contentShape(Rectangle())
  }

  /// Background color for the icon.
  var backgroundColor: Color {
    isPressed ? Color(.systemGray4) : (highlighted ? .blueHalo : .groupedSecondaryBackground)
  }

  /// View representing the background of the icon.
  @ViewBuilder
  var iconBackground: some View {
    ZStack {
      RoundedRectangle(cornerRadius: Dimensions.cornerRadius)
        .foregroundColor(backgroundColor)
      if highlighted {
        RoundedRectangle(cornerRadius: Dimensions.cornerRadius)
          .stroke(Color.chromeBlue, lineWidth: 2)
      }
    }
  }

  /// Icon for the destination.
  var icon: some View {
    let interiorPadding: CGFloat
    switch layoutParameters {
    case .vertical(_, let iconPadding):
      interiorPadding = iconPadding
    case .horizontal:
      interiorPadding = Dimensions.horizontalLayoutIconPadding
    }
    let symbolName = destination.symbolName ?? "gearshape"
    let image = (destination.systemSymbol ? Image(systemName: symbolName) : Image(symbolName))
      .renderingMode(.template)
    return iconBuilder(interiorPadding: interiorPadding, image: image)
      .overlay {
        GeometryReader { geometry in
          Color.clear.preference(
            key: IconFramePreferenceKey.self, value: geometry.frame(in: .named(Self.viewNamespace)))
        }
      }
  }

  var circleBadge: some View {
    return Circle()
      .frame(width: Dimensions.badgeWidth, height: Dimensions.badgeWidth)
      .offset(
        x: Dimensions.iconWidth - (Dimensions.badgeWidth / 2),
        y: -Dimensions.iconWidth + (Dimensions.badgeWidth / 2))
  }

  var sealBadge: some View {
    Image(systemName: "seal.fill")
      .resizable()
      .foregroundColor(.blue600)
      .frame(width: Dimensions.newLabelBadgeWidth, height: Dimensions.newLabelBadgeWidth)
      .overlay {
        if let newLabelString = L10nUtils.stringWithFixup(
          messageId: IDS_IOS_NEW_LABEL_FEATURE_BADGE)
        {
          Text(newLabelString)
            .font(.system(size: 10, weight: .bold, design: .rounded))
            .scaledToFit()
            .foregroundColor(.primaryBackground)
        }
      }
      .offset(
        x: Dimensions.iconWidth - (Dimensions.newLabelBadgeWidth - 10),
        y: -Dimensions.iconWidth + (Dimensions.newLabelBadgeWidth - 10))
  }

  /// Build the image to be displayed, based on the configuration of the item.
  /// TODO(crbug.com/40833570): Remove this once only the symbols are present.
  @ViewBuilder
  func iconBuilder(interiorPadding: CGFloat, image: Image) -> some View {
    let configuredImage = image.overlay {
      if destination.badge == .error {
        circleBadge.foregroundColor(.red500)
      } else if destination.badge == .promo {
        circleBadge.foregroundColor(.blue600)
      } else if destination.badge == .new {
        sealBadge
      }
    }
    .frame(width: Dimensions.imageWidth, height: Dimensions.imageWidth)
    .padding(interiorPadding)
    .background(iconBackground)
    // Without explicitly removing the image from accessibility,
    // VoiceOver will occasionally read out icons it thinks it can
    // recognize.
    .accessibilityHidden(true)

    configuredImage.foregroundColor(.blue600).imageScale(.medium).font(
      Font.system(size: Dimensions.iconSymbolSize, weight: .medium)
    )
    .alignmentGuide(.icon) { $0[VerticalAlignment.center] }
    .alignmentGuide(HorizontalAlignment.editButton) { $0[.leading] }
    .alignmentGuide(VerticalAlignment.editButton) { $0[.top] }
  }

  /// Text view for the destination.
  var text: some View {
    // Only the vertical layout has extra spacing around the text
    let textSpacing: CGFloat
    let maximumLines: Int?
    switch layoutParameters {
    case .vertical:
      textSpacing = Dimensions.verticalLayoutTextPadding
      maximumLines = 2
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

  var accessibilityLabel: String {
    return [
      destination.name,
      destination.badge == .error
        ? L10nUtils.stringWithFixup(
          messageId: IDS_IOS_ITEM_ACCOUNT_ERROR_BADGE_ACCESSIBILITY_HINT) : nil,
      destination.badge == .promo
        ? L10nUtils.stringWithFixup(messageId: IDS_IOS_NEW_ITEM_ACCESSIBILITY_HINT) : nil,
      destination.badge == .new
        ? L10nUtils.stringWithFixup(messageId: IDS_IOS_TOOLS_MENU_CELL_NEW_FEATURE_BADGE) : nil,
    ].compactMap { $0 }.joined(separator: ", ")
  }

  var accessibilityIdentifier: String {
    return [
      destination.accessibilityIdentifier,
      destination.badge == .error ? AccessibilityIdentifier.errorBadge : nil,
      destination.badge == .promo ? AccessibilityIdentifier.promoBadge : nil,
      destination.badge == .new ? AccessibilityIdentifier.newBadge : nil,
    ].compactMap { $0 }.joined(separator: "-")
  }

  static public func verticalLayoutDestinationWidth(iconSpacing: CGFloat, iconPadding: CGFloat)
    -> CGFloat
  {
    return Dimensions.imageWidth + 2 * iconSpacing + 2 * iconPadding
  }
}
