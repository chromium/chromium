// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

/// Struct that contains constants used in the Tab Strip UI.
struct TabStripConstants {

  /// Collection view constants.
  struct CollectionView {
    static let tabStripCellReuseIdentifier = "tabStripCellReuseIdentifier"
    static let tabStripCellPrefixIdentifier = "tabStripCellPrefixIdentifier"
    static let topInset: CGFloat = 4
    static let horizontalInset: CGFloat = 16
  }

  /// Tab item constants.
  struct TabItem {
    static let height: CGFloat = 40
    static let minWidth: CGFloat = 132
    static let maxWidth: CGFloat = 233
    static let horizontalSpacing: CGFloat = 6
    static let leadingSeparatorMinInset: CGFloat = 8
    static let horizontalInset: CGFloat = 4
  }

  /// New tab button constants.
  struct NewTabButton {
    static let accessibilityIdentifier: String = "TabStripNewTabButtonAccessibilityIdentifier"
    static let width: CGFloat = 46
    static let topInset: CGFloat = 4
    static let bottomInset: CGFloat = 8
    static let leadingInset: CGFloat = 4
    static let trailingInset: CGFloat = 10
    static let cornerRadius: CGFloat = 16
    static let symbolPointSize: CGFloat = 16
  }

  /// Separator view constants.
  struct SeparatorView {
    static let viewHeight: CGFloat = 40
    static let separatorWidth: CGFloat = 2
    static let separatorCornerRadius: CGFloat = 1
    static let smallSeparatorHeight: CGFloat = 12
    static let reuglarSeparatorHeight: CGFloat = 18
    static let horizontalInset: CGFloat = 4
    static let leadingInset: CGFloat = 6
  }

}
