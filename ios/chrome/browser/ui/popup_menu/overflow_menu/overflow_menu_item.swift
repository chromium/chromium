// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents an item in the overflow menu.
@objcMembers public class OverflowMenuItem: NSObject, ObservableObject {
  public enum ImageType {
    case uiImage(UIImage)
    case name(String)
  }
  /// The user-visible name of the item.
  @Published public var name: String

  /// The base `UIImage` used to load the image for SwiftUI.
  @Published public var storedImage: UIImage

  /// The SwiftUI `Image` for the action icon.
  public var image: Image {
    return Image(uiImage: storedImage)
  }

  /// The accessibility identifier for this item.
  @Published public var accessibilityIdentifier: String

  /// Whether the action is disabled by enterprise policy.
  @Published public var enterpriseDisabled: Bool

  /// Closure to execute when item is selected.
  @Published public var handler: () -> Void

  public init(
    name: String, image: UIImage, accessibilityIdentifier: String, enterpriseDisabled: Bool,
    handler: @escaping () -> Void
  ) {
    self.name = name
    storedImage = image
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
