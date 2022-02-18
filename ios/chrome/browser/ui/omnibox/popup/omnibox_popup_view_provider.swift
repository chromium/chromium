// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit

// A provider to provide the SwiftUI PopupView to Objective-C. This is
// necessary because Objective-C can't see SwiftUI types.
@objcMembers public class OmniboxPopupViewProvider: NSObject {
  public static func makeViewController(withModel model: PopupModel) -> UIViewController {
    return UIHostingController(rootView: PopupView(model: model))
  }
}
