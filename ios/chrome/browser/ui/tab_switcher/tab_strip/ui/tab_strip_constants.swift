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
    static let scrollDelayAfterInsert: DispatchTimeInterval = .milliseconds(500)
  }

  /// Tab item constants.
  struct TabItem {
    static let height: CGFloat = 40
    static let minWidth: CGFloat = 132
    static let maxWidth: CGFloat = 233
    static let horizontalSpacing: CGFloat = 6
    static let leadingSeparatorMinInset: CGFloat = 8
    static let horizontalInset: CGFloat = 4
    static let horizontalSelectedInset: CGFloat = 4
    static let selectedZIndex: Int = 10
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

  /// Animated separator constants.
  struct AnimatedSeparator {
    static let regularSeparatorHeight: CGFloat = 18
    static let minSeparatorHeight: CGFloat = 12
    // The Cell separator is animated below this threshold.
    static let collapseHorizontalInsetThreshold: CGFloat = 8
    static let collapseHorizontalInset: CGFloat = 6
  }

  /// Static separator constants.
  struct StaticSeparator {
    static let viewHeight: CGFloat = 36
    static let separatorWidth: CGFloat = 2
    static let separatorCornerRadius: CGFloat = 1
    static let smallSeparatorHeight: CGFloat = 12
    static let regularSeparatorHeight: CGFloat = 18
    static let horizontalInset: CGFloat = 4
    static let leadingInset: CGFloat = 6
    static let bottomInset: CGFloat = 4
    static let backgroundColorAlpha: CGFloat = 0.3
  }

}
