// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Additional data associated with a `TabStripItemIdentifier`.
@objc class TabStripItemData: NSObject {

  // Color of the group stroke.
  // If this is a tab, then this is the color of that tab's group, if there is one.
  // Otherwise if this is a group, then this would be the color of that group.
  @objc public var groupStrokeColor: UIColor?

  // Whether this is the first tab in its group.
  // If this item is not a tab or does not belong to a group, then this should be false.
  @objc public var isFirstTabInGroup: Bool = false

  // Whether this is the last tab in its group.
  // If this item is not a tab or does not belong to a group, then this should be false.
  @objc public var isLastTabInGroup: Bool = false
}
