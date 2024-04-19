// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import UIKit

// A provider to provide the SwiftUI PriceHistory to Objective C. This is
// necessary because Objective C can't see SwiftUI types.
@available(iOS 16, *)
@objcMembers class PriceHistoryProvider: NSObject {
  public static func makeViewController(withHistory history: [Date: NSNumber]) -> UIViewController {
    return UIHostingController(rootView: HistoryGraph())
  }
}
