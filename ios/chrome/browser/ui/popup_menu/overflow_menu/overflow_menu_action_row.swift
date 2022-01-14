// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// A view that displays an action in the overflow menu.
struct OverflowMenuActionRow: View {
  /// The action for this row.
  @ObservedObject var action: OverflowMenuAction

  var body: some View {
    Button(
      action: action.handler,
      label: {
        HStack {
          Text(action.name)
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
    .accentColor(.cr_textPrimaryColor)
  }
}
