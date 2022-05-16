// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents a footer in the overflow menu.
@objcMembers public class OverflowMenuFooter: OverflowMenuItem {

  /// Text for the link to learn more about enterprise policies.
  @Published public var link: String

  public init(
    name: String, link: String, imageName: String, accessibilityIdentifier: String,
    handler: @escaping () -> Void
  ) {
    self.link = link
    super.init(
      name: name, image: .name(imageName), accessibilityIdentifier: accessibilityIdentifier,
      enterpriseDisabled: false,
      handler: handler)
  }
}
