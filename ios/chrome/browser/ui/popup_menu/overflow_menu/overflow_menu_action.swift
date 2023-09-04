// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Combine

/// Represents an action in the overflow menu.
@objcMembers public class OverflowMenuAction: OverflowMenuItem {
  /// Whether the action is enabled. This does not take enterprise policies
  /// into account.
  @Published public var enabled = true

  /// If true, do not override the color of the row's text with a custom color.
  @Published public var useSystemRowColoring = false

  /// Whether the action should be highlighted in the UI.
  @Published public var highlighted = false

  /// The uniquely-identifying `overflow_menu::ActionType` of the action,
  /// stored as an int because Swift does not yet support C++ enum variables.
  public var actionType = 0
}
