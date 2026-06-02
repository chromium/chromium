// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit
import ios_chrome_common_ui_colors_swift

/// A view that displays the identity/signed-in state in the overflow menu.
struct OverflowMenuIdentityRow: View {
  /// Spacing between the sign-in icon and the text vertical stack.
  private static let imageSpacing: CGFloat = 12

  /// Spacing between the title and subtitle.
  private static let textSpacing: CGFloat = 4

  /// The maximum number of lines for the title.
  private static let titleLineLimit = 2

  /// Vertical padding for the row content.
  private static let verticalPadding: CGFloat = 6

  /// Trailing padding for the row content to adjust spacing.
  private static let trailingPadding: CGFloat = -4

  /// Width and height of the sign-in icon/avatar.
  private static let imageSize: CGFloat = 40

  /// Font size of the fallback sign-in symbol.
  private static let symbolPointSize: CGFloat = 26

  /// The action for this row.
  @ObservedObject var action: OverflowMenuAction

  weak var metricsHandler: PopupMenuMetricsHandler?

  var body: some View {
    Button(
      action: {
        metricsHandler?.popupMenuTriggerElement()
        metricsHandler?.popupMenuUserSelectedAction()
        metricsHandler?.popupMenuDidTriggerAction(action.actionType)
        action.handler()
      },
      label: {
        HStack(alignment: .center, spacing: Self.imageSpacing) {
          signinIcon
          VStack(alignment: .leading, spacing: Self.textSpacing) {
            Text(action.name)
              .font(.headline)
              .lineLimit(Self.titleLineLimit)
            if let subtitle = action.subtitle {
              Text(subtitle)
                .font(.subheadline)
                .foregroundColor(.textSecondary)
                .lineLimit(nil)
                .multilineTextAlignment(.leading)
            }
          }
          Spacer()
        }
        .padding([.vertical], Self.verticalPadding)
        .padding([.trailing], Self.trailingPadding)
        .contentShape(Rectangle())
      }
    )
    .accentColor(.textPrimary)
    .listRowBackground(background)
    .accessibilityIdentifier(action.accessibilityIdentifier)
    .disabled(!action.enabled || action.enterpriseDisabled)
    .listRowSeparatorTint(.overflowMenuSeparator)
  }

  @ViewBuilder
  private var signinIcon: some View {
    if let avatarImage = action.image {
      Image(uiImage: avatarImage)
        .resizable()
        .aspectRatio(contentMode: .fill)
        .frame(width: Self.imageSize, height: Self.imageSize)
        .clipShape(Circle())
        .accessibilityHidden(true)
    } else if let symbolName = action.symbolName {
      let symbol = action.systemSymbol ? Image(systemName: symbolName) : Image(symbolName)
      symbol
        .font(Font.system(size: Self.symbolPointSize, weight: .medium))
        .imageScale(.large)
        .frame(width: Self.imageSize, height: Self.imageSize, alignment: .center)
        .foregroundColor(.chromeBlue)
        .accessibilityHidden(true)
    }
  }

  /// The background color for this row.
  var background: some View {
    let color =
      action.highlighted
      ? Color("destination_highlight_color") : Color(.secondarySystemGroupedBackground)
    return color.animation(.default)
  }
}
