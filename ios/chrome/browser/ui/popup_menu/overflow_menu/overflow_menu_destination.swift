// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents a destination in the overflow menu.
@objcMembers public class OverflowMenuDestination: OverflowMenuItem {

  @objc public enum BadgeType: Int {
    case none
    // Whether the destination should show an error badge.
    case error
    // Whether the destination should show a promo badge.
    case promo
    // Whether the destination should show a "New" badge
    // indicating a new destination.
    case new
  }

  public var badge: BadgeType = .none

  /// The uniquely-identifying `overflow_menu::Destination` of the destination.
  public var destination: Int = 0
}
