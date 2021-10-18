// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents an action in the overflow menu.
@objcMembers public class OverflowMenuAction: OverflowMenuItem {
  /// Whether the action is disabled for non-enterprise reasons.
  @Published public var disabled = false
}
