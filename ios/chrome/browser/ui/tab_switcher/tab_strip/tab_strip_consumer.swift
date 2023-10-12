// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@objc protocol TabStripConsumer {

  /// Tells the consumer to replace its current set of items with `items` and
  /// update the selected item to be `selectedItem`.
  func populate(items: [TabSwitcherItem]?, selectedItem: TabSwitcherItem?)

  /// Reloads `item`'s content.
  func reloadItem(_ item: TabSwitcherItem?)

  /// Tells the consumer to select `item`.
  func selectItem(_ item: TabSwitcherItem?)

}
