// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tab strip has two types of items: tabs and groups.
// This should only be used in Objective-C, as Swift code can use enum
// `TabStripItemIdentifier.Item` instead.
@objc enum TabStripItemType: Int {
  case tab
  case group
}

// Represents tab strip items in a diffable data source. TabStripItemIdentifier
// equality is based on the type and the potential item's properties. This means
// that two different TabStripItemIdentifier-s can be equal (via -isEqual:) and
// share the same -hash. Different items though won't be equal and will likely
// have different hashes (the hashing for tabs is based on NSNumber's hashing,
// which prevents consecutive identifiers to have consecutive hash values, while
// the hashing for groups is based on NSValue's hashing of the TabGroup
// pointer).
@objc class TabStripItemIdentifier: NSObject, NSCopying {

  // Item this identifier is referring to.
  // Since a tab strip item can either be a tab (represented by `TabSwitcherItem`)
  // or a group (represented by `TabGroupItem`), a tab strip item is represented
  // by the sum of these two types i.e. Item = TabSwitcherItem + TabGroupItem.
  // In Swift, sum types are enumerations with associated values, which are not
  // visible from Objective-C code, hence the need for `TabStripItemType`.
  enum Item: Hashable {
    case tab(TabSwitcherItem)
    case group(TabGroupItem)
  }

  // Item this identifier is referring to, either a `.tab(_)` or a `.group(_)`.
  public let item: Item
  // Hash of the item.
  private let itemHash: Int

  // MARK: - Initialization

  public init(_ tabSwitcherItem: TabSwitcherItem) {
    self.item = .tab(tabSwitcherItem)
    self.itemHash = Int(GetHashForTabSwitcherItem(tabSwitcherItem))
  }

  public init(_ tabGroupItem: TabGroupItem) {
    self.item = .group(tabGroupItem)
    self.itemHash = Int(GetHashForTabGroupItem(tabGroupItem))
  }

  public convenience init?(_ tabSwitcherItem: TabSwitcherItem?) {
    guard let tabSwitcherItem = tabSwitcherItem else { return nil }
    self.init(tabSwitcherItem)
  }

  public convenience init?(_ tabGroupItem: TabGroupItem?) {
    guard let tabGroupItem = tabGroupItem else { return nil }
    self.init(tabGroupItem)
  }

  // MARK: - NSObject

  // TODO(crbug.com/329073651): Refactor -hash and -isEqual.
  public override func isEqual(_ object: Any?) -> Bool {
    guard let object = object as? TabStripItemIdentifier else {
      return false
    }
    switch (item, object.item) {
    case (.tab(let lhs), .tab(let rhs)):
      return CompareTabSwitcherItems(lhs, rhs)
    case (.group(let lhs), .group(let rhs)):
      return CompareTabGroupItems(lhs, rhs)
    default:
      return false
    }
  }

  // TODO(crbug.com/329073651): Refactor -hash and -isEqual.
  public override var hash: Int {
    return itemHash
  }

  public override var description: String {
    switch item {
    case .tab(let tabSwitcherItem):
      return tabSwitcherItem.description
    case .group(let tabGroupItem):
      return tabGroupItem.description
    }
  }

  // MARK: - NSCopying

  public func copy(with zone: NSZone? = nil) -> Any {
    switch item {
    case .tab(let tabSwitcherItem):
      return TabStripItemIdentifier(tabSwitcherItem)
    case .group(let tabGroupItem):
      return TabStripItemIdentifier(tabGroupItem)
    }
  }

  // MARK: - Objective-C interface

  // Factory method to create an identifier for a `TabSwitcherItem`.
  @objc public static func tabIdentifier(_ tabSwitcherItem: TabSwitcherItem?)
    -> TabStripItemIdentifier?
  {
    return TabStripItemIdentifier(tabSwitcherItem)
  }

  // Factory method to create an identifier for a `TabGroupItem`.
  @objc public static func groupIdentifier(_ tabGroupItem: TabGroupItem?) -> TabStripItemIdentifier?
  {
    return TabStripItemIdentifier(tabGroupItem)
  }

  // Only valid when `itemType`` is `TabStripItemTypeTab`.
  @objc public var tabSwitcherItem: TabSwitcherItem? {
    switch item {
    case .tab(let tabSwitcherItem):
      return tabSwitcherItem
    case .group(_):
      return nil
    }
  }

  // Only valid when `itemType`` is `TabStripItemTypeGroup`.
  @objc public var tabGroupItem: TabGroupItem? {
    switch item {
    case .tab(_):
      return nil
    case .group(let tabGroupItem):
      return tabGroupItem
    }
  }

  // The type of tab strip item this identifier is referring to.
  @objc public var itemType: TabStripItemType {
    switch item {
    case .tab(_):
      return .tab
    case .group(_):
      return .group
    }
  }
}
