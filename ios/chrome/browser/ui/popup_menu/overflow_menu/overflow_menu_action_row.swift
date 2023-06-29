// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// A view that displays an action in the overflow menu.
@available(iOS 15, *)
struct OverflowMenuActionRow: View {
  /// Remove some of the default padding on the row, as it is too large by
  /// default.
  private static let rowEndPadding: CGFloat = -4

  /// Add extra padding between the row content and move handle in edit mode.
  private static let editRowEndPadding: CGFloat = 8

  /// The size of the "N" IPH icon.
  private static let newLabelIconWidth: CGFloat = 15

  /// The action for this row.
  @ObservedObject var action: OverflowMenuAction

  weak var metricsHandler: PopupMenuMetricsHandler?

  @Environment(\.editMode) var editMode

  private var isEditing: Bool {
    return editMode?.wrappedValue.isEditing ?? false
  }

  var body: some View {
    Button(
      action: {
        guard !isEditing else {
          return
        }
        metricsHandler?.popupMenuTookAction()
        action.handler()
      },
      label: {
        rowContent
          .contentShape(Rectangle())
      }
    )
    .accessibilityIdentifier(action.accessibilityIdentifier)
    .disabled(!action.enabled || action.enterpriseDisabled)
    .if(!action.useSystemRowColoring) { view in
      view.accentColor(.textPrimary)
    }
    .listRowSeparatorTint(.overflowMenuSeparator)
  }

  @ViewBuilder
  private var rowContent: some View {
    if isEditing {
      HStack {
        rowIcon
        name
        Spacer()
        Toggle(isOn: $action.shown.animation()) {}
          .labelsHidden()
          .tint(.chromeBlue)
      }
      .padding([.trailing], Self.editRowEndPadding)
    } else {
      HStack {
        name
        if action.displayNewLabelIcon {
          newLabelIconView
        }
        if let rowIcon = rowIcon {
          Spacer()
          rowIcon
        }
      }
      .padding([.trailing], Self.rowEndPadding)
    }
  }

  private var name: some View {
    Text(action.name).lineLimit(1)
  }

  private var rowIcon: OverflowMenuRowIcon? {
    action.symbolName.flatMap { symbolName in
      OverflowMenuRowIcon(
        symbolName: symbolName, systemSymbol: action.systemSymbol,
        monochromeSymbol: action.monochromeSymbol)
    }
  }

  // The "N" IPH icon view.
  private var newLabelIconView: some View {
    Image(systemName: "seal.fill")
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
      .accessibilityIdentifier("overflowRowIPHBadgeIdentifier")
  }
}
