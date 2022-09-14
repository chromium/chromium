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
          Spacer()
          action.image
            // Without explicitly removing the image from accessibility,
            // VoiceOver will occasionally read out icons it thinks it can
            // recognize.
            .accessibilityHidden(true)
        }
        .contentShape(Rectangle())
      }
    )
    .accessibilityIdentifier(action.accessibilityIdentifier)
    .disabled(!action.enabled || action.enterpriseDisabled)
    .accentColor(.textPrimary)
    .listRowSeparatorTint(.overflowMenuSeparator)
  }
}
