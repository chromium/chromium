// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Combine

/// Holds all the data necessary to create the views for the overflow menu.
@objcMembers public class OverflowMenuModel: NSObject, ObservableObject {
  /// The destinations for the overflow menu.
  @Published public var destinations: [OverflowMenuDestination]

  /// The action groups for the overflow menu.
  public var actionGroups: [OverflowMenuActionGroup]

  public init(
    destinations: [OverflowMenuDestination],
    actionGroups: [OverflowMenuActionGroup]
  ) {
    self.destinations = destinations
    self.actionGroups = actionGroups
  }
}
