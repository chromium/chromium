// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Holds UI data necessary to display the overflow menu.
@objcMembers public class OverflowMenuUIConfiguration: NSObject, ObservableObject {
  @Published public var presentingViewControllerHorizontalSizeClass: UserInterfaceSizeClass
  @Published public var presentingViewControllerVerticalSizeClass: UserInterfaceSizeClass

  @Published public var highlightDestinationsRow = false

  /// The destination list's frame in screen coordinates.
  public var destinationListScreenFrame: CGRect = .zero

  public init(
    presentingViewControllerHorizontalSizeClass: UIUserInterfaceSizeClass,
    presentingViewControllerVerticalSizeClass: UIUserInterfaceSizeClass
  ) {
    self.presentingViewControllerHorizontalSizeClass =
      UserInterfaceSizeClass(presentingViewControllerHorizontalSizeClass) ?? .compact
    self.presentingViewControllerVerticalSizeClass =
      UserInterfaceSizeClass(presentingViewControllerVerticalSizeClass) ?? .compact
  }
}
