// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A SwiftUI view for the overflow menu displaying a subsection of the actions list.
@available(iOS 15, *)
struct OverflowMenuActionSection: View {

  enum Dimensions {
    // Default height if no other header or footer. This spaces the sections
    // out properly.
    static let headerFooterHeight: CGFloat = 20
  }

  @ObservedObject var actionGroup: OverflowMenuActionGroup

  weak var metricsHandler: PopupMenuMetricsHandler?

  var body: some View {
    Section(
      content: {
        ForEach(actionGroup.actions) { action in
          OverflowMenuActionRow(action: action, metricsHandler: metricsHandler)
        }
      },
      header: {
        Spacer()
          .frame(height: Dimensions.headerFooterHeight)
          .listRowInsets(EdgeInsets())
      },
      footer: {
        if let actionFooter = actionGroup.footer {
          OverflowMenuFooterRow(footer: actionFooter)
        } else {
          Spacer()
            // Use `leastNonzeroMagnitude` to remove the footer. Otherwise,
            // it uses a default height.
            .frame(height: CGFloat.leastNonzeroMagnitude)
            .listRowInsets(EdgeInsets())
        }
      })
  }
}
