// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import WidgetKit

extension WidgetConfiguration {
  func crDisfavoredLocations() -> some WidgetConfiguration {
    return disfavoredLocations(
      [.iPhoneWidgetsOnMac], for: [.systemSmall, .systemMedium, .accessoryCircular])
  }
}
