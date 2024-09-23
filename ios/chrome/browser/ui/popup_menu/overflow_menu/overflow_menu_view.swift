// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

struct OverflowMenuView: View {
  @ObservedObject var model: OverflowMenuModel

  var uiConfiguration: OverflowMenuUIConfiguration

  weak var metricsHandler: PopupMenuMetricsHandler?

  /// The namespace for the animation of this view appearing or disappearing.
  let namespace: Namespace.ID

  var body: some View {
    GeometryReader { geometry in
      VStack(
        alignment: .leading,
        // Leave no spaces above or below Divider, the two other sections will
        // include proper spacing.
        spacing: 0
      ) {
        OverflowMenuDestinationList(
          destinations: $model.destinations,
          width: geometry.size.width,
          metricsHandler: metricsHandler,
          uiConfiguration: uiConfiguration, namespace: namespace
        )
        .matchedGeometryEffect(id: MenuCustomizationAnimationID.destinations, in: namespace)
        Divider()
        OverflowMenuActionList(
          actionGroups: model.actionGroups, metricsHandler: metricsHandler,
          uiConfiguration: uiConfiguration, namespace: namespace)
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
