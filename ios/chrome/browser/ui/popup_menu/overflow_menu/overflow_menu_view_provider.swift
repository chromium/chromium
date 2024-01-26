// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit

// A provider to provide the SwiftUI OverflowMenuView to Objective C. This is
// necessary because Objective C can't see SwiftUI types.
@objcMembers public class OverflowMenuViewProvider: NSObject {
  public static func makeViewController(
    withModel model: OverflowMenuModel,
    uiConfiguration: OverflowMenuUIConfiguration,
    metricsHandler: PopupMenuMetricsHandler,
    customizationEventHandler: MenuCustomizationEventHandler?
  ) -> UIViewController {
    return OverflowMenuHostingController(
      rootView: OverflowMenuContainerView(
        model: model, uiConfiguration: uiConfiguration, metricsHandler: metricsHandler,
        customizationEventHandler: customizationEventHandler),
      uiConfiguration: uiConfiguration)
  }
}
