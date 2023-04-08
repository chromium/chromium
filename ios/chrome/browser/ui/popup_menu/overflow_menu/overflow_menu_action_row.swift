// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// A view that displays an action in the overflow menu.
@available(iOS 15, *)
struct OverflowMenuActionRow: View {
  /// The action for this row.
  @ObservedObject var action: OverflowMenuAction

  /// The size of the symbols.
  static let symbolSize: CGFloat = 18
  static let symbolImageFrameLength: CGFloat = 30
  static let symbolImagePadding: CGFloat = -4

  /// The size of the "N" IPH icon.
  static let newLabelIconWidth: CGFloat = 15

  weak var metricsHandler: PopupMenuMetricsHandler?

  var body: some View {
    Button(
      action: {
        metricsHandler?.popupMenuTookAction()
        action.handler()
      },
      label: {
        HStack {
          Text(action.name).lineLimit(1)
          if action.displayNewLabelIcon {
            newLabelIconView()
              .accessibilityIdentifier("overflowRowIPHBadgeIdentifier")
          }
          Spacer()
          imageBuilder().frame(
            width: OverflowMenuActionRow.symbolImageFrameLength,
            height: OverflowMenuActionRow.symbolImageFrameLength, alignment: .center
          )
          // Without explicitly removing the image from accessibility,
          // VoiceOver will occasionally read out icons it thinks it can
          // recognize.
          .accessibilityHidden(true)
        }
        .contentShape(Rectangle())
      }
    ).padding([.trailing], OverflowMenuActionRow.symbolImagePadding)
      .accessibilityIdentifier(action.accessibilityIdentifier)
      .disabled(!action.enabled || action.enterpriseDisabled)
      .accentColor(.textPrimary)
      .listRowSeparatorTint(.overflowMenuSeparator)
  }

  /// Build the image to be displayed, based on the configuration of the item.
  /// TODO(crbug.com/1315544): Remove this once only the symbols are present.
  @ViewBuilder
  func imageBuilder() -> some View {
    actionSymbol().font(Font.system(size: OverflowMenuActionRow.symbolSize, weight: .medium))
      .imageScale(
        .medium)
  }

  func actionSymbol() -> Image {
    let symbol =
      action.systemSymbol ? Image(systemName: action.symbolName) : Image(action.symbolName)
    if action.monochromeSymbol {
      return symbol.symbolRenderingMode(.monochrome)
    }
    return symbol
  }

  // Returns the "N" IPH icon view.
  func newLabelIconView() -> some View {
    return Image(systemName: "seal.fill")
      .resizable()
      .foregroundColor(.blue600)
      .frame(
        width: OverflowMenuActionRow.newLabelIconWidth,
        height: OverflowMenuActionRow.newLabelIconWidth
      )
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
  }
}
