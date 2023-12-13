// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

/// Struct that contains constants used in the Tab Strip UI.
struct TabStripConstants {

  /// Tab item constants.
  struct TabItem {
    static let height: CGFloat = 39
    static let minWidth: CGFloat = 80
    static let maxWidth: CGFloat = 150
    static let horizontalSpacing: CGFloat = 0
    static let selectedZindex: Int = 10
  }

  /// New tab button constants.
  struct NewTabButton {
    static let contentInset: CGFloat = 6
    static let symbolPointSize: CGFloat = 18
  }

}
