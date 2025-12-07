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

  static func badgeTypeFrom(string: String) -> BadgeType {
    switch string {
    case "none":
      return .none
    case "error":
      return .error
    case "promo":
      return .promo
    case "new":
      return .new
    default:
      return .none
    }
  }

  static func stringFrom(badgeType: BadgeType) -> String {
    switch badgeType {
    case .none:
      return "none"
    case .error:
      return "error"
    case .promo:
      return "promo"
    case .new:
      return "new"
    }
  }

  public var badge: BadgeType = .none

  /// Whether this destination can be hidden, or if it must always be shown.
  @Published public var canBeHidden = true

  /// The uniquely-identifying `overflow_menu::Destination` of the destination.
  public var destination: Int = 0
}
