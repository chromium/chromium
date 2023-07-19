// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

@available(iOS 15, *)
struct OverflowMenuView: View {
  @ObservedObject var model: OverflowMenuModel

  var uiConfiguration: OverflowMenuUIConfiguration

  weak var metricsHandler: PopupMenuMetricsHandler?

  var body: some View {
    GeometryReader { geometry in
      VStack(
        alignment: .leading,
        // Leave no spaces above or below Divider, the two other sections will
        // include proper spacing.
        spacing: 0
      ) {
        OverflowMenuDestinationList(
          destinations: $model.destinations, metricsHandler: metricsHandler,
          uiConfiguration: uiConfiguration, dragHandler: nil
        ).frame(height: OverflowMenuListStyle.destinationListHeight)
        Divider()
        OverflowMenuActionList(actionGroups: model.actionGroups, metricsHandler: metricsHandler)
        // Add a spacer on iPad to make sure there's space below the list.
        if uiConfiguration.presentingViewControllerHorizontalSizeClass == .regular
          && uiConfiguration.presentingViewControllerVerticalSizeClass == .regular
        {
          Spacer()
        }
      }
      .background(Color(.systemGroupedBackground).edgesIgnoringSafeArea(.all))
      .onPreferenceChange(OverflowMenuDestinationList.HighlightedDestinationBounds.self) { pref in
        if let pref = pref {
          uiConfiguration.highlightedDestinationFrame = geometry[pref]
        }
      }
    }
  }
}
