// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Combine
import SwiftUI

/// Model class for holding and tracking action customization occuring in the
/// overflow menu.
@objcMembers
public class ActionCustomizationModel: NSObject, ObservableObject {
  @Published public private(set) var actionsGroup: OverflowMenuActionGroup

  public var shownActions: [OverflowMenuAction] {
    return actionsGroup.actions.filter(\.shown)
  }

  public var hiddenActions: [OverflowMenuAction] {
    return actionsGroup.actions.filter { !$0.shown }
  }

  private let initialData: [(action: OverflowMenuAction, shown: Bool)]

  public var hasChanged: Bool {
    zip(initialData, actionsGroup.actions).map { initial, current in
      initial.action == current && initial.shown == current.shown
    }.contains { !$0 }
  }

  /// Holds sinks for all the action observation.
  var cancellables: Set<AnyCancellable> = []

  public init(actions: [OverflowMenuAction]) {
    let actionsGroup = OverflowMenuActionGroup(
      groupName: "actions", actions: actions, footer: nil)
    actionsGroup.supportsReordering = true
    self.actionsGroup = actionsGroup

    // Store initial shown states for each action so changes to that can be
    // tracked.
    initialData = actions.map { (action: $0, shown: $0.shown) }

    super.init()

    /// Listen to the action arrays of the two groups changing so that this
    /// object can update when that happens. Making `OverflowMenuActionGroup` a
    /// `struct` would be a better solution to this issue. Then as a reference
    /// type, whenever its members change, it changes and this object can
    /// publish that change. However, `OverflowMenuActionGroup` is used from
    /// Objective-C, so that doesn't work.
    self.actionsGroup.objectWillChange.sink { [weak self] _ in
      self?.objectWillChange.send()
    }.store(in: &cancellables)
    self.actionsGroup.actions.forEach { action in
      action.objectWillChange.sink { [weak self] _ in
        self?.objectWillChange.send()
      }.store(in: &cancellables)
    }
  }
}
