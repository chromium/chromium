// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit

// A provider to provide the SwiftUI OverflowMenuView to Objective C. This is
// necessary because Objective C can't see SwiftUI types.
@available(iOS 15, *)
@objcMembers public class OverflowMenuViewProvider: NSObject {
  public static func makeViewController(
    withModel model: OverflowMenuModel,
    uiConfiguration: OverflowMenuUIConfiguration,
    metricsHandler: PopupMenuMetricsHandler,
    carouselMetricsDelegate: PopupMenuCarouselMetricsDelegate
  ) -> UIViewController {
    return OverflowMenuHostingController(
      rootView: OverflowMenuView(
        model: model, uiConfiguration: uiConfiguration, metricsHandler: metricsHandler,
        carouselMetricsDelegate: carouselMetricsDelegate),
      uiConfiguration: uiConfiguration)
  }
}
