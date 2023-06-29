// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Combine
import SwiftUI

/// Model class for holding and tracking action customization occuring in the
/// overflow menu.
@objcMembers
public class ActionCustomizationModel: NSObject, ObservableObject {
  @Published public var shownActions: OverflowMenuActionGroup
  @Published public var hiddenActions: OverflowMenuActionGroup

  /// Holds sinks for all the action observation.
  var cancellables: Set<AnyCancellable> = []

  public init(actions: [OverflowMenuAction]) {
    let splitActions = Self.splitActions(actions)
    let shownActions = OverflowMenuActionGroup(
      groupName: "shown", actions: splitActions.shown, footer: nil)
    shownActions.supportsReordering = true
    self.shownActions = shownActions
    self.hiddenActions = OverflowMenuActionGroup(
      groupName: "hidden", actions: splitActions.hidden, footer: nil)

    super.init()

    // Set up sinks for every action so when their toggle value changes, this
    // class can reassign them to the correct group.
    actions.forEach { action in
      action.$shown.sink { [weak self] newShown in
        self?.toggle(action: action, newShown: newShown)
      }.store(in: &cancellables)
    }
  }

  /// Splits an initial actions array into separate arrays based on their
  /// `.shown` value.
  static func splitActions(_ actions: [OverflowMenuAction]) -> (
    shown: [OverflowMenuAction], hidden: [OverflowMenuAction]
  ) {
    var shown: [OverflowMenuAction] = []
    var hidden: [OverflowMenuAction] = []

    for action in actions {
      if action.shown {
        shown.append(action)
      } else {
        hidden.append(action)
      }
    }
    return (shown: shown, hidden: hidden)
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
