// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Combine
import SwiftUI

/// Model class for holding and tracking action customization occuring in the
/// overflow menu.
@objcMembers
public class ActionCustomizationModel: NSObject, ObservableObject {
  @Published public private(set) var shownActions: OverflowMenuActionGroup
  @Published public private(set) var hiddenActions: OverflowMenuActionGroup

  private let initialData: (shown: [OverflowMenuAction], hidden: [OverflowMenuAction])

  public var hasChanged: Bool {
    return initialData.shown != shownActions.actions || initialData.hidden != hiddenActions.actions
  }

  /// Holds sinks for all the action observation.
  var cancellables: Set<AnyCancellable> = []

  public init(actions: [OverflowMenuAction]) {
    let shownActions = actions.filter { $0.shown }
    let hiddenActions = actions.filter { !$0.shown }

    let shownActionsGroup = OverflowMenuActionGroup(
      groupName: "shown", actions: shownActions, footer: nil)
    shownActionsGroup.supportsReordering = true
    self.shownActions = shownActionsGroup
    self.hiddenActions = OverflowMenuActionGroup(
      groupName: "hidden", actions: hiddenActions, footer: nil)

    initialData = (shown: shownActions, hidden: hiddenActions)

    super.init()

    // Set up sinks for every action so when their toggle value changes, this
    // class can reassign them to the correct group.
    actions.forEach { action in
      // dropFirst, so the sink is only called for subsequent changes, not the
      // initial state.
      action.$shown.dropFirst().sink { [weak self] newShown in
        self?.toggle(action: action, newShown: newShown)
      }.store(in: &cancellables)
    }

    /// Listen to the action arrays of the two groups changing so that this
    /// object can update when that happens. Making `OverflowMenuActionGroup` a
    /// `struct` would be a better solution to this issue. Then as a reference
    /// type, whenever its members change, it changes and this object can
    /// publish that change. However, `OverflowMenuActionGroup` is used from
    /// Objective-C, so that doesn't work.
    self.shownActions.objectWillChange.sink { [weak self] _ in
      self?.objectWillChange.send()
    }.store(in: &cancellables)
    self.hiddenActions.objectWillChange.sink { [weak self] _ in
      self?.objectWillChange.send()
    }.store(in: &cancellables)
  }

  /// Moves `action` to the correct group based on its new `shown`
  /// state.
  func toggle(action: OverflowMenuAction, newShown: Bool) {
    // If action is now shown, remove it from hiddenActions if necessary
    // and add to shownActions. Otherwise, do the reverse.
    if newShown {
      guard let index = hiddenActions.actions.firstIndex(of: action) else {
        return
      }
      hiddenActions.actions.remove(at: index)
      shownActions.actions.append(action)
    } else {
      guard let index = shownActions.actions.firstIndex(of: action) else {
        return
      }
      shownActions.actions.remove(at: index)
      hiddenActions.actions.append(action)
    }
  }
}
