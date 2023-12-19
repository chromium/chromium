// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

/// Struct that contains constants used in the Tab Strip UI.
struct TabStripConstants {

  /// Collection view constants.
  struct CollectionView {
    static let inset: CGFloat = 4
  }

  /// Tab item constants.
  struct TabItem {
    static let height: CGFloat = 40
    static let minWidth: CGFloat = 132
    static let maxWidth: CGFloat = 233
    static let horizontalSpacing: CGFloat = 0
    static let selectedZindex: Int = 10
  }

  /// New tab button constants.
  struct NewTabButton {
    static let width: CGFloat = 46

    static let topInset: CGFloat = 4
    static let bottomInset: CGFloat = 8
    static let leadingInset: CGFloat = 4
    static let trailingInset: CGFloat = 10

    static let cornerRadius: CGFloat = 16
    static let symbolPointSize: CGFloat = 16
  }

}
