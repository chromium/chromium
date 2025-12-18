// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents an item in the overflow menu.
@objcMembers public class OverflowMenuItem: NSObject, ObservableObject {
  /// The user-visible name of the item.
  @Published public var name: String

  /// The name of the symbol to be used.
  @Published public var symbolName: String?

  /// Whether the symbol is a system one (or a custom one).
  @Published public var systemSymbol: Bool

  /// Whether the symbol is monochrome or default configuration.
  @Published public var monochromeSymbol: Bool

  /// The accessibility identifier for this item.
  @Published public var accessibilityIdentifier: String

  /// Whether the action is disabled by enterprise policy.
  @Published public var enterpriseDisabled: Bool

  /// Whether the action should display the "N" IPH icon.
  @Published public var displayNewLabelIcon: Bool

  /// Closure to execute when item is selected.
  @Published public var handler: () -> Void

  /// Whether the item is shown or hidden in the menu overall.
  @Published public var shown = true {
    didSet {
      onShownToggleCallback()
    }
  }

  /// A callback, called whenever `shown` is changed.
  @Published public var onShownToggleCallback: () -> Void = {}

  @Published public var longPressItems: [OverflowMenuLongPressItem] = []

  public init(
    name: String,
    symbolName: String?,
    systemSymbol: Bool,
    monochromeSymbol: Bool,
    accessibilityIdentifier: String,
    enterpriseDisabled: Bool,
    displayNewLabelIcon: Bool,
    handler: @escaping () -> Void
  ) {
    self.name = name
    self.symbolName = symbolName
    self.systemSymbol = systemSymbol
    self.monochromeSymbol = monochromeSymbol
    self.accessibilityIdentifier = accessibilityIdentifier
    self.enterpriseDisabled = enterpriseDisabled
    self.displayNewLabelIcon = displayNewLabelIcon
    self.handler = handler
  }
}

/// Represents the data necessary to add a long press context menu to an item.
@objcMembers public class OverflowMenuLongPressItem: NSObject, ObservableObject {
  @Published public var title: String
  @Published public var symbolName: String
  @Published public var handler: () -> Void
  public init(title: String, symbolName: String, handler: @escaping () -> Void) {
    self.title = title
    self.symbolName = symbolName
    self.handler = handler
  }
}

// MARK: - Identifiable

extension OverflowMenuItem: Identifiable {
  public var id: String {
    return name
  }
}

extension OverflowMenuLongPressItem: Identifiable {
  public var id: String {
    return title
  }
}
