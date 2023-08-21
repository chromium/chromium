// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// The view holding the menu and deciding whether to show the regular menu
/// view or the version for when customization is enabled. Customization can
/// be enabled by calling the appropriate methods on the provided model.
struct OverflowMenuContainerView: View {
  @ObservedObject var model: OverflowMenuModel

  var uiConfiguration: OverflowMenuUIConfiguration

  weak var metricsHandler: PopupMenuMetricsHandler?

  weak var customizationEventHandler: MenuCustomizationEventHandler?

  /// The namespace for the animation that transitions between the two views.
  @Namespace private var namespace

  var body: some View {
    Group {
      if let customization = model.customization {
        MenuCustomizationView(
          actionCustomizationModel: customization.actions,
          destinationCustomizationModel: customization.destinations,
          uiConfiguration: uiConfiguration, eventHandler: customizationEventHandler,
          namespace: namespace)
      } else {
        OverflowMenuView(
          model: model, uiConfiguration: uiConfiguration, metricsHandler: metricsHandler,
          namespace: namespace)
      }
    }
    .animation(.default, value: model.customization)
  }
}
