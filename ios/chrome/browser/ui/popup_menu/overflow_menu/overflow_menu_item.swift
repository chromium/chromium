// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents an item in the overflow menu.
@objcMembers public class OverflowMenuItem: NSObject, ObservableObject {
  /// The user-visible name of the item.
  @Published public var name: String

  /// The UIImage used to load the image for SwiftUI.
  @Published public var uiImage: UIImage

  /// The SwiftUI `Image` for the action icon.
  public var image: Image {
    return Image(uiImage: uiImage)
  }

  /// The accessibility identifier for this item.
  @Published public var accessibilityIdentifier: String

  /// Whether the action is disabled by enterprise policy.
  @Published public var enterpriseDisabled: Bool

  /// Closure to execute when item is selected.
  @Published public var handler: () -> Void

  public init(
    name: String, uiImage: UIImage, accessibilityIdentifier: String, enterpriseDisabled: Bool,
    handler: @escaping () -> Void
  ) {
    self.name = name
    self.uiImage = uiImage
    self.accessibilityIdentifier = accessibilityIdentifier
    self.enterpriseDisabled = enterpriseDisabled
    self.handler = handler
  }
}

// MARK: - Identifiable

extension OverflowMenuItem: Identifiable {
  public var id: String {
    return name
  }
}
