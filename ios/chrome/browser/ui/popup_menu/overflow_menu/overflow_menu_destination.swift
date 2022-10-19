// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents a destination in the overflow menu.
@objcMembers public class OverflowMenuDestination: OverflowMenuItem {

  @objc public enum BadgeType: Int {
    // Whether the destination should show a badge.
    case blueDot
    // Whether the destination should show a "New" badge
    // indicating a new destination.
    case newLabel
    case none
  }

  public var badge: BadgeType = .none

  /// The uniquely-identifying name of the destination.
  public var destinationName: String = ""
}
