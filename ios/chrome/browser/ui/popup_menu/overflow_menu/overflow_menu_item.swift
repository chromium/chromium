// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents an item in the overflow menu.
@objcMembers public class OverflowMenuItem: NSObject, ObservableObject {
  /// The user-visible name of the item.
  @Published public var name: String

  /// The name of the symbol to be used.
  @Published public var symbolName: String

  /// Whether the symbol is a system one (or a custom one).
  @Published public var systemSymbol: Bool

  /// Whether the symbol is monochrome or default configuration.
  @Published public var monochromeSymbol: Bool

  /// The base `UIImage` used to load the image for SwiftUI.
  /// TODO(crbug.com/1315544): Remove this once the symbols have shipped.
  @Published public var storedImage: UIImage

  /// The SwiftUI `Image` for the action icon.
  /// TODO(crbug.com/1315544): Remove this once the symbols have shipped.
  public var image: Image {
    return Image(uiImage: self.storedImage)
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
    symbolName = ""
    systemSymbol = false
    monochromeSymbol = false
    self.accessibilityIdentifier = accessibilityIdentifier
    self.enterpriseDisabled = enterpriseDisabled
    self.handler = handler
  }

  public init(
    name: String, symbolName: String, systemSymbol: Bool, monochromeSymbol: Bool,
    accessibilityIdentifier: String,
    enterpriseDisabled: Bool,
    handler: @escaping () -> Void
  ) {
    self.name = name
    self.storedImage = UIImage()
    self.symbolName = symbolName
    self.systemSymbol = systemSymbol
    self.monochromeSymbol = monochromeSymbol
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
