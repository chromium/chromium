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
    let enabled = action.enabled && !action.enterpriseDisabled
    Button(
      action: {
        if enabled {
          action.handler()
        }
      },
      label: {
        HStack {
          Text(action.name)
            .opacity(enabled ? 1 : 0.5)
          Spacer()
          action.image
            .opacity(enabled ? 1 : 0.5)
        }
        .contentShape(Rectangle())
      }
    )
    .accentColor(.cr_textPrimaryColor)
  }
}
