// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A SwiftUI view for the overflow menu displaying a subsection of the actions list.
struct OverflowMenuActionSection<FooterBackground: View>: View {

  // Default height if no other header or footer. This spaces the sections
  // out properly.
  static var headerFooterHeight: CGFloat { 20 }

  @ObservedObject var actionGroup: OverflowMenuActionGroup

  weak var metricsHandler: PopupMenuMetricsHandler?

  // A custom background for the footer if provided.
  let footerBackground: FooterBackground

  init(
    actionGroup: OverflowMenuActionGroup, metricsHandler: PopupMenuMetricsHandler? = nil,
    @ViewBuilder footerBackground: () -> FooterBackground = { EmptyView() }
  ) {
    self.actionGroup = actionGroup
    self.metricsHandler = metricsHandler
    self.footerBackground = footerBackground()
  }

  var body: some View {
    Section(
      content: {
        ForEach(actionGroup.actions) { action in
          OverflowMenuActionRow(action: action, metricsHandler: metricsHandler)
            .moveDisabled(!actionGroup.supportsReordering)
        }
        .onMove(perform: move)
      },
      header: {
        Spacer()
          .frame(height: Self.headerFooterHeight)
          .listRowInsets(EdgeInsets())
          .accessibilityHidden(true)
      },
      footer: {
        if let actionFooter = actionGroup.footer {
          OverflowMenuFooterRow(footer: actionFooter)
            .background {
              footerBackground
            }
        } else {
          Spacer()
            // Use `leastNonzeroMagnitude` to remove the footer. Otherwise,
            // it uses a default height.
            .frame(height: CGFloat.leastNonzeroMagnitude)
            .listRowInsets(EdgeInsets())
            .accessibilityHidden(true)
            .background {
              footerBackground
            }
        }
      })
  }

  func move(fromOffsets offsets: IndexSet, toOffset destination: Int) {
    actionGroup.actions.move(fromOffsets: offsets, toOffset: destination)
  }
}
