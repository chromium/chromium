// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Holds UI data necessary to display the overflow menu.
@objcMembers public class OverflowMenuUIConfiguration: NSObject, ObservableObject {
  @Published public var presentingViewControllerHorizontalSizeClass: UserInterfaceSizeClass
  @Published public var presentingViewControllerVerticalSizeClass: UserInterfaceSizeClass

  @Published public var highlightDestinationsRow = false
  /// The integer value matches overflow_menu::Destination, setting it to -1 will not highlight any.
  @Published public var highlightDestination: Int = -1

  @Published public var scrollToAction: OverflowMenuAction? = nil

  /// The highlighted destination's frame, in the coordinate system of the menu view.
  public var highlightedDestinationFrame: CGRect = .zero

  /// The destination list's frame in screen coordinates.
  public var destinationListScreenFrame: CGRect = .zero

  static public func numDestinationsVisibleWithoutHorizontalScrolling(
    forScreenWidth width: CGFloat, forContentSizeCategory sizeCategory: UIContentSizeCategory
  ) -> CGFloat {
    let contentSizeCategory = ContentSizeCategory(sizeCategory) ?? .medium

    return OverflowMenuDestinationList.numDestinationsVisibleWithoutHorizontalScrolling(
      forScreenWidth: width, forSizeCategory: contentSizeCategory)
  }

  public init(
    presentingViewControllerHorizontalSizeClass: UIUserInterfaceSizeClass,
    presentingViewControllerVerticalSizeClass: UIUserInterfaceSizeClass,
    highlightDestination: Int
  ) {
    self.presentingViewControllerHorizontalSizeClass =
      UserInterfaceSizeClass(presentingViewControllerHorizontalSizeClass) ?? .compact
    self.presentingViewControllerVerticalSizeClass =
      UserInterfaceSizeClass(presentingViewControllerVerticalSizeClass) ?? .compact
    self.highlightDestination = highlightDestination
  }
}
