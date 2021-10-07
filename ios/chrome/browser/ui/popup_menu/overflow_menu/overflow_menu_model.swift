// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Holds all the data necessary to create the views for the overflow menu.
@objcMembers public class OverflowMenuModel: NSObject, ObservableObject {
  /// The destinations for the overflow menu.
  public var destinations: [OverflowMenuDestination]

  /// The destinations for the overflow menu.
  public var actions: [OverflowMenuAction]

  public init(
    destinations: [OverflowMenuDestination],
    actions: [OverflowMenuAction]
  ) {
    self.destinations = destinations
    self.actions = actions
  }
}
