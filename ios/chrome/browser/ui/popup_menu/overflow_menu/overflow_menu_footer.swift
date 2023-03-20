// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents a footer in the overflow menu.
@objcMembers public class OverflowMenuFooter: OverflowMenuItem {

  /// Text for the link to learn more about enterprise policies.
  @Published public var link: String

  /// The image for this footer.
  public var image: Image

  public init(
    name: String, link: String, image: UIImage, accessibilityIdentifier: String,
    handler: @escaping () -> Void
  ) {
    self.link = link
    self.image = Image(uiImage: image)
    super.init(
      name: name, symbolName: "", systemSymbol: true, monochromeSymbol: true,
      accessibilityIdentifier: accessibilityIdentifier,
      enterpriseDisabled: false, displayNewLabelIcon: false,
      handler: handler)
  }
}
