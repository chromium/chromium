// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents a destination in the overflow menu.
@objcMembers public class OverflowMenuDestination: OverflowMenuItem {
  // Whether the destination should show a badge.
  public var showBadge: Bool = false

  /// The uniquely-identifying name of the destination.
  public var destinationName: String = ""
}
