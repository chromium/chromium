// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

/// Struct that contains constants used in the Tab Strip UI.
public struct TabStripConstants {

  /// Collection view constants.
  public struct CollectionView {
    public static let tabStripTabCellReuseIdentifier = "tabStripTabCellReuseIdentifier"
    public static let tabStripTabCellPrefixIdentifier = "tabStripTabCellPrefixIdentifier"
    public static let tabStripGroupCellPrefixIdentifier = "tabStripGroupCellPrefixIdentifier"
    public static let topInset: CGFloat = 4
    public static let horizontalInset: CGFloat = 16
    public static let scrollDelayAfterInsert: DispatchTimeInterval = .milliseconds(500)
    public static let groupStrokeLineWidth: CGFloat = 2
    public static let groupStrokeExtension: CGFloat = 17
  }

  /// Tab item constants.
  public struct TabItem {
    public static let height: CGFloat = 40
    public static let minWidth: CGFloat = 132
    public static let minWidthV3: CGFloat = 164
    public static let closeButtonVisibilityWidth: CGFloat = 30
    public static let maxWidth: CGFloat = 233
    public static let horizontalSpacing: CGFloat = 6
    public static let leadingSeparatorMinInset: CGFloat = 8
    public static let horizontalInset: CGFloat = 4
    public static let horizontalSelectedInset: CGFloat = 4
    public static let selectedZIndex: Int = 10
    public static let fontSize: CGFloat = 14
    public static let maximumVisibleDistance: CGFloat = 10
    public static let closeButtonAccessibilityIdentifier: String =
      "TabStripCloseButtonAccessibilityIdentifier"
  }

  /// Group item constants.
  public struct GroupItem {
    public static let height: CGFloat = TabItem.height
    public static let titleContainerHorizontalPadding: CGFloat = 10
    public static let titleContainerHorizontalMargin: CGFloat = 4
    public static let fontSize: CGFloat = TabItem.fontSize
    public static let maxTitleWidth: CGFloat = 150
    public static let minCellWidth =
      titleContainerHorizontalPadding * 2 + titleContainerHorizontalMargin * 2
    public static let maxCellWidth = maxTitleWidth + minCellWidth
  }

  /// New tab button constants.
  public struct NewTabButton {
    public static let accessibilityIdentifier: String =
      "TabStripNewTabButtonAccessibilityIdentifier"
    public static let width: CGFloat = 46
    public static let topInset: CGFloat = 4
    public static let bottomInset: CGFloat = 8
    public static let leadingInset: CGFloat = 4
    public static let trailingInset: CGFloat = 10
    public static let diameter: CGFloat = 36
    public static let highContrastCornerRadius: CGFloat = 11
    public static let legacyCornerRadius: CGFloat = 16
    public static let symbolPointSize: CGFloat = 16
    public static let symbolBiggerPointSize: CGFloat = 18
    public static let constraintUpdateAnimationDuration: CGFloat = 0.3
  }

  /// Animated separator constants.
  public struct AnimatedSeparator {
    public static let regularSeparatorHeight: CGFloat = 18
    public static let minSeparatorHeight: CGFloat = 12
    // The Cell separator is animated below this threshold.
    public static let collapseHorizontalInsetThreshold: CGFloat = 8
    public static let collapseHorizontalInset: CGFloat = 6
  }

  /// Static separator constants.
  public struct StaticSeparator {
    public static let viewHeight: CGFloat = 36
    public static let separatorWidth: CGFloat = 2
    public static let separatorCornerRadius: CGFloat = 1
    public static let smallSeparatorHeight: CGFloat = 12
    public static let regularSeparatorHeight: CGFloat = 18
    public static let horizontalInset: CGFloat = 4
    public static let leadingInset: CGFloat = 6
    public static let bottomInset: CGFloat = 4
    public static let backgroundColorAlpha: CGFloat = 0.3
    public static let solidBackgroundVerticalPadding: CGFloat = 5
  }

}
