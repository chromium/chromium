// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A model class representing a group of `OverflowMenuActions`.
@objcMembers public class OverflowMenuActionGroup: NSObject, ObservableObject {

  /// An internal name for the group. This is not displayed to the user, but is
  /// used to identify the group.
  public let groupName: String

  /// The actions for this group.
  @Published public var actions: [OverflowMenuAction]

  /// Whether the group can be reordered by the user.
  @Published public var supportsReordering = false

  /// The footer at bottom.
  public var footer: OverflowMenuFooter?

  public init(groupName: String, actions: [OverflowMenuAction], footer: OverflowMenuFooter?) {
    self.groupName = groupName
    self.actions = actions
    self.footer = footer
  }

  /// Sets the actions wrapped in a SwiftUI `withAnimation` block.
  /// `withAnimation` is otherwise inaccessible to Objective-C.
  public func setActionsWithAnimation(_ actions: [OverflowMenuAction]) {
    withAnimation {
      self.actions = actions
    }
  }
}

// MARK: - Identifiable

extension OverflowMenuActionGroup: Identifiable {
  public var id: String {
    return groupName
  }
}
