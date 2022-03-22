// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

@objcMembers public class OmniboxPedalData: NSObject, OmniboxPedal {
  public let hint: String
  public let action: () -> Void

  public init(hint: String, action: @escaping () -> Void) {
    self.hint = hint
    self.action = action
  }
}
