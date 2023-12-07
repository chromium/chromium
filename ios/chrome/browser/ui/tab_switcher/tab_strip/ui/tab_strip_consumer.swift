// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Consumer protocol for the TabStrip.
@objc protocol TabStripConsumer {

  /// Tells the consumer to replace its current set of items with `items` and updates the selected
  /// item to be `selectedItem`. The selected item must be in the `items`.
  func populate(items: [TabSwitcherItem]?, selectedItem: TabSwitcherItem?)

  /// Tells the consumer to select `item`.
  func selectItem(_ item: TabSwitcherItem?)

  /// Reloads `item`'s content.
  func reloadItem(_ item: TabSwitcherItem?)

  /// Replaces `oldItem` by `newItem`.
  /// The nullability is here for Objective-C compatibility. If one of them is nil, the consumer will do nothing.
  func replaceItem(_ oldItem: TabSwitcherItem?, withItem newItem: TabSwitcherItem?)

}
