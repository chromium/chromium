// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents an action in the overflow menu.
@objcMembers public class OverflowMenuAction: OverflowMenuItem {
  /// Whether the action is enabled. This does not take enterprise policies
  /// into account.
  @Published public var enabled = true
}
