// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Delegate for the TabStrip view controller.
@objc protocol TabStripViewControllerDelegate {

  /// Ask the delegate to share the item.
  func tabStrip(
    _ tabStrip: TabStripViewController?, shareItem: TabSwitcherItem?, originView: UIView?)

}
