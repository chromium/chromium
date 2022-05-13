// Copyright 2021 The Chromium Authors. All rights reserved.
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

  /// The base data used to load the image for SwiftUI, either a `UImage` or
  /// a `String`.
  /// Note that SwiftUI has a bug regarding `Image`s generated from `UIImage`s
  /// in Dark Mode. The image color will not adjust.
  @Published public var storedImage: ImageType

  /// The SwiftUI `Image` for the action icon.
  public var image: Image {
    switch storedImage {
    case .uiImage(let uiImage):
      return Image(uiImage: uiImage)
    case .name(let name):
      return Image(name)
    }
  }

  /// The accessibility identifier for this item.
  @Published public var accessibilityIdentifier: String

  /// Whether the action is disabled by enterprise policy.
  @Published public var enterpriseDisabled: Bool

  /// Closure to execute when item is selected.
  @Published public var handler: () -> Void

  public init(
    name: String, image: ImageType, accessibilityIdentifier: String, enterpriseDisabled: Bool,
    handler: @escaping () -> Void
  ) {
    self.name = name
    storedImage = image
    self.accessibilityIdentifier = accessibilityIdentifier
    self.enterpriseDisabled = enterpriseDisabled
    self.handler = handler
  }

  public convenience init(
    name: String, uiImage: UIImage, accessibilityIdentifier: String, enterpriseDisabled: Bool,
    handler: @escaping () -> Void
  ) {
    self.init(
      name: name, image: .uiImage(uiImage), accessibilityIdentifier: accessibilityIdentifier,
      enterpriseDisabled: enterpriseDisabled, handler: handler)
  }

  public convenience init(
    name: String, imageName: String, accessibilityIdentifier: String, enterpriseDisabled: Bool,
    handler: @escaping () -> Void
  ) {
    self.init(
      name: name, image: .name(imageName), accessibilityIdentifier: accessibilityIdentifier,
      enterpriseDisabled: enterpriseDisabled, handler: handler)
  }

  /// Objective-C-exposed method to change `storedImage`, as Objective-C cannot
  /// view enums with stored values.
  @objc(setStordUIImage:) public func setStoredImage(uiImage: UIImage) {
    storedImage = .uiImage(uiImage)
  }

  /// Objective-C-exposed method to change `storedImage`, as Objective-C cannot
  /// view enums with stored values.
  @objc(setStoredImageName:) public func setStoredImage(name: String) {
    storedImage = .name(name)
  }
}

// MARK: - Identifiable

extension OverflowMenuItem: Identifiable {
  public var id: String {
    return name
  }
}
