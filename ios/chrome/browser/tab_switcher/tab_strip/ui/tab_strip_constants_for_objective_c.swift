// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import ios_chrome_browser_tab_switcher_tab_strip_ui_swift_constants

/// Bridge to access `TabStripConstants.CollectionView` from Objective-C code.
@objcMembers public class TabStripCollectionViewConstants: NSObject {
  public static let tabStripGroupCellPrefixIdentifier = TabStripConstants.CollectionView
    .tabStripGroupCellPrefixIdentifier
  public static let tabStripTabCellPrefixIdentifier = TabStripConstants.CollectionView
    .tabStripTabCellPrefixIdentifier
  public static let height = TabStripConstants.CollectionView
    .height
  public static let groupStrokeLineWidth = TabStripConstants.CollectionView.groupStrokeLineWidth
  public static let groupStrokeExtension = TabStripConstants.CollectionView.groupStrokeExtension
}

/// Bridge to access `TabStripConstants.TabItem` from Objective-C code.
@objcMembers public class TabStripTabItemConstants: NSObject {
  public static let fontSize = TabStripConstants.TabItem.fontSize
  public static let selectedZIndex = TabStripConstants.TabItem.selectedZIndex
  public static let horizontalSpacing = TabStripConstants.TabItem.horizontalSpacing
  public static let maxWidth = TabStripConstants.TabItem.maxWidth
  public static let closeButtonAccessibilityIdentifier = TabStripConstants.TabItem
    .closeButtonAccessibilityIdentifier
  public static let blueDotAccessibilityIdentifier = TabStripConstants.TabItem
    .blueDotAccessibilityIdentifier
}

/// Bridge to access `TabStripConstants.GroupItem` from Objective-C code.
@objcMembers public class TabStripGroupItemConstants: NSObject {
  public static let contentContainerHorizontalPadding = TabStripConstants.GroupItem
    .contentContainerHorizontalPadding
  public static let contentContainerHorizontalMargin = TabStripConstants.GroupItem
    .contentContainerHorizontalMargin
  public static let fontSize = TabStripConstants.GroupItem.fontSize
  public static let maxTitleWidth = TabStripConstants.GroupItem.maxTitleWidth
  public static let notificationDotAccessibilityIdentifier = TabStripConstants.GroupItem
    .notificationDotAccessibilityIdentifier
  public static let maxCellWidth = TabStripConstants.GroupItem.maxCellWidth
}

/// Bridge to access `TabStripConstants.StaticSeparator` from Objective-C code.
@objcMembers public class TabStripStaticSeparatorConstants: NSObject {
  public static let separatorWidth = TabStripConstants.StaticSeparator.separatorWidth
  public static let regularSeparatorHeight = TabStripConstants.StaticSeparator
    .regularSeparatorHeight
  public static let separatorCornerRadius = TabStripConstants.StaticSeparator.separatorCornerRadius
}
