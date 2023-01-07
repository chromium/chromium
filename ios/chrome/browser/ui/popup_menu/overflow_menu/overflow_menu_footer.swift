// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents a footer in the overflow menu.
@objcMembers public class OverflowMenuFooter: OverflowMenuItem {

  /// Text for the link to learn more about enterprise policies.
  @Published public var link: String

  public init(
    name: String, link: String, image: UIImage, accessibilityIdentifier: String,
    handler: @escaping () -> Void
  ) {
    self.link = link
    super.init(
      name: name, image: image, accessibilityIdentifier: accessibilityIdentifier,
      enterpriseDisabled: false,
      handler: handler)
  }
}
