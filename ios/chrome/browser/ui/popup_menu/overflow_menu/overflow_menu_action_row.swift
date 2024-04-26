// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// Custom toggle style for Overflow Menu Action rows, consisting of a circle
/// border when the toggle is off and a circle with checkmark when the toggle
/// is on.
struct OverflowMenuActionToggleStyle: ToggleStyle {
  static let onStyle = AnyShapeStyle(.tint)
  static let offStyle = AnyShapeStyle(Color.grey500)

  @ViewBuilder
  func makeBody(configuration: Configuration) -> some View {
    Button {
      configuration.isOn.toggle()
    } label: {
      Label {
        configuration.label
      } icon: {
        Image(systemName: configuration.isOn ? "checkmark.circle.fill" : "circle")
          .foregroundStyle(configuration.isOn ? Self.onStyle : Self.offStyle)
          .imageScale(.large)
      }
    }
    .overflowMenuActionToggleCompat()
  }
}

extension View {
  /// For whatever reason, in iOS 15, the button is not toggleable unless this
  /// `buttonStyle` is set. In iOS 16+, the `buttonStyle` is not necessary.
  fileprivate func overflowMenuActionToggleCompat() -> some View {
    if #available(iOS 16.0, *) {
      return self
    }
    return self.buttonStyle(.borderless)
  }
}

/// A view that displays an action in the overflow menu.
struct OverflowMenuActionRow: View {
  /// Remove some of the default padding on the row, as it is too large by
  /// default.
  private static let rowEndPadding: CGFloat = -4

  /// Add extra padding between the row content and move handle in edit mode.
  private static let editRowEndPadding: CGFloat = 8

  /// The size of the "N" IPH icon.
  private static let newLabelIconWidth: CGFloat = 15

  // The duration that the view's highlight should persist.
  private static let highlightDuration: DispatchTimeInterval = .seconds(2)

  /// The action for this row.
  @ObservedObject var action: OverflowMenuAction

  weak var metricsHandler: PopupMenuMetricsHandler?

  @Environment(\.editMode) var editMode

  private var isEditing: Bool {
    return editMode?.wrappedValue.isEditing ?? false
  }

  var body: some View {
    button
      .listRowBackground(background)
      .onChange(of: action.highlighted) { _ in
        guard action.automaticallyUnhighlight else {
          return
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + Self.highlightDuration) {
          action.highlighted = false
        }
      }
      .onAppear {
        if action.highlighted && action.automaticallyUnhighlight {
          DispatchQueue.main.asyncAfter(deadline: .now() + Self.highlightDuration) {
            action.highlighted = false
          }
        }
      }
      .accessibilityIdentifier(action.accessibilityIdentifier)
      .disabled(!action.enabled || action.enterpriseDisabled)
      .if(!isEditing) { view in
        view.contextMenu {
          ForEach(action.longPressItems) { item in
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
      .if(!action.useButtonStyling) { view in
        view.accentColor(.textPrimary)
      }
      .listRowSeparatorTint(.overflowMenuSeparator)
  }

  @ViewBuilder
  private var rowContent: some View {
    if isEditing {
      HStack {
        Toggle(isOn: $action.shown.animation()) {
          Text(action.name)
        }
        .toggleStyle(OverflowMenuActionToggleStyle())
        .labelStyle(.iconOnly)
        .tint(.chromeBlue)
        .accessibilityRemoveTraits(.isSelected)
        rowIcon
        centerTextView
        Spacer()
      }
      .padding([.trailing], Self.editRowEndPadding)
      .accessibilityElement(children: .combine)
    } else {
      HStack {
        // If there is no icon, the text should be centered.
        if rowIcon == nil {
          Spacer()
        }
        centerTextView
        if action.displayNewLabelIcon {
          newLabelIconView
        }
        Spacer()
        if let rowIcon = rowIcon {
          rowIcon
        }
      }
      .padding([.trailing], Self.rowEndPadding)
    }
  }

  /// The row's middle text content
  @ViewBuilder
  private var centerTextView: some View {
    VStack(alignment: .leading, spacing: 4) {
      name
      subtitle
    }
  }

  // The button view, which is replaced by just a plain view when this is in
  // edit mode.
  @ViewBuilder
  var button: some View {
    if isEditing {
      rowContent
    } else {
      Button(
        action: {
          metricsHandler?.popupMenuTookAction()
          metricsHandler?.popupMenuUserSelectedAction()
          action.handler()
        },
        label: {
          rowContent
            .contentShape(Rectangle())
        }
      )
      .if(action.useButtonStyling) { view in
        view.buttonStyle(.borderless)
      }
    }
  }

  private var name: some View {
    Text(action.name).lineLimit(2)
  }

  @ViewBuilder
  private var subtitle: some View {
    if let subtitle = action.subtitle {
      Text(subtitle).lineLimit(1).font(.caption).foregroundColor(.textTertiary)
    }
  }

  private var rowIcon: OverflowMenuRowIcon? {
    action.symbolName.flatMap { symbolName in
      OverflowMenuRowIcon(
        symbolName: symbolName, systemSymbol: action.systemSymbol,
        monochromeSymbol: action.monochromeSymbol)
    }
  }

  /// The background color for this row.
  var background: some View {
    let color =
      action.highlighted
      ? Color("destination_highlight_color") : Color(.secondarySystemGroupedBackground)
    // `.listRowBackground cannot be animated, so apply the animation to the color directly.
    return color.animation(.default)
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
