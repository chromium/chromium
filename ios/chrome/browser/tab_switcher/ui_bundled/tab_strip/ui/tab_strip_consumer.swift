// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Consumer protocol for the TabStrip.
@objc protocol TabStripConsumer {

  /// Tells the consumer to replace its current set of items with `items`, item data with `itemData`
  /// and updates the selected item to be `selectedItem`. The selected item must be in the `items`.
  /// All tab items following a group item are set as children of that group up until the first tab for which
  /// `isLastTabInGroup` is true.
  func populate(
    items: [TabStripItemIdentifier]?, selectedItem: TabSwitcherItem?,
    itemData: [TabStripItemIdentifier: TabStripItemData],
    itemParents: [TabStripItemIdentifier: TabGroupItem])

  /// Tells the consumer to select `item`.
  func selectItem(_ item: TabSwitcherItem?)

  /// Reconfigure the content of cells associated with `items`.
  func reconfigureItems(_ items: [TabStripItemIdentifier])

  /// Moves `item`  before `destinationItem`. Pass nil to insert at the end.
  /// If `destinationItem` is a tab item which is inside of a group, then `item` will move to that group.
  func moveItem(
    _ item: TabStripItemIdentifier, beforeItem destinationItem: TabStripItemIdentifier?)

  /// Moves `item`  after `destinationItem`. Pass nil to insert at the beginning.
  /// If `destinationItem` is a tab item which is inside of a group, then `item` will move to that group.
  func moveItem(
    _ item: TabStripItemIdentifier, afterItem destinationItem: TabStripItemIdentifier?)

  /// Moves `item` to the last position in the children of `parentItem`.
  func moveItem(
    _ item: TabStripItemIdentifier, insideGroup parentItem: TabGroupItem)

  /// Inserts `items` before `destinationItem`. Pass nil to insert at the end.
  /// It's an error if any of the `items` is already passed to the consumer (and not yet removed).
  /// If `destinationItem` is a tab item which is inside of a group, then `items` will be inserted in that group.
  func insertItems(
    _ items: [TabStripItemIdentifier], beforeItem destinationItem: TabStripItemIdentifier?)

  /// Inserts `items` after `destinationItem`. Pass nil to insert at the beginning.
  /// It's an error if any of the `items` is already passed to the consumer (and not yet removed).
  /// If `destinationItem` is a tab item which is inside of a group, then `items` will be inserted in that group.
  func insertItems(
    _ items: [TabStripItemIdentifier], afterItem destinationItem: TabStripItemIdentifier?)

  /// Inserts `items` at the last position in the children of `parentItem`.
  /// It's an error if any of the `items` is already passed to the consumer (and not yet removed).
  func insertItems(
    _ items: [TabStripItemIdentifier], insideGroup parentItem: TabGroupItem)

  /// Removes `items`.
  func removeItems(_ items: [TabStripItemIdentifier]?)

  /// Replaces `oldItem` by `newItem`.
  /// The nullability is here for Objective-C compatibility. If one of them is nil, the consumer will do nothing.
  func replaceItem(_ oldItem: TabSwitcherItem?, withItem newItem: TabSwitcherItem?)

  /// Updates the `TabStripItemData` associated with tab strip items.
  func updateItemData(
    _ updatedItemData: [TabStripItemIdentifier: TabStripItemData], reconfigureItems: Bool)

  /// Collapses `group` so as to make its children hidden.
  func collapseGroup(_ group: TabGroupItem)

  /// Expands `group` so as to make its children visible.
  func expandGroup(_ group: TabGroupItem)

}
