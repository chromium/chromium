// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit

/// A provider to provide the SwiftUI PopupView to Objective-C. This is
/// necessary because Objective-C can't see SwiftUI types.
///
/// - Parameters:
///   - model: The popup model to be used by the popup view.
///   - popupShouldSelfSize: Whether the popup should resize itself to fit its content.
/// - Returns: The hosting controller which embeds the popup view.
@objcMembers public class OmniboxPopupViewProvider: NSObject {
  public static func makeViewController(withModel model: PopupModel, popupShouldSelfSize: Bool)
    -> UIViewController & ContentProviding
  {
    return OmniboxPopupHostingController(
      rootView: PopupView(
        model: model, shouldSelfSize: popupShouldSelfSize,
        appearanceContainerType: OmniboxPopupHostingController.self))
  }
}

class OmniboxPopupHostingController: UIHostingController<PopupView>, ContentProviding {
  override func viewDidLoad() {
    view.backgroundColor = .clear
    view.isOpaque = false
  }

  var hasContent: Bool {
    return rootView.model.sections.map(\.matches.count).reduce(0, +) > 0
  }
}
